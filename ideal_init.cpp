/*
 * Ideal init
 *
 * Copyright (C) 2015 JackpotClavin <jonclavin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdarg>

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ideal_init.h"

#define MKDIR 0
#define CHMOD 1
#define CHOWN 2
#define WRITE 3
#define SYMLINK 4
#define MOUNT 5
#define SETRLIMIT 6
#define SETPROP 7
#define ECHO 8
#define SERVICE 9

#define DEFAULT_INIT_FILE_DIR "default_init_files"
#define DEVICE_INIT_FILE_DIR "device_init_files"

std::unordered_set<std::string> default_init_registry;
std::unordered_set<std::string> device_setprop_registry;
std::unordered_map<std::string, std::string> symlink_registry;
std::unordered_set<std::string> taken_symlinks;

std::string services_list;

// this function reads in the default ramdisk's system calls, and parses them
// so that they are always delimited by a single space, and compatible with the
// information we will use later in the program.
std::string standardize_string(std::string in, int syscall) {

    std::string ret;
    std::stringstream token(in);
    bool first = true;
    int count = 0;

    std::string tmp;
    int octal;

    while (token >> tmp) {
        if (!first)
            ret += " ";
        first = false;

        if ((syscall == CHMOD && count == 1) || (syscall == MKDIR && count == 2)) {
            //turns '0755' into just '755'
            octal = std::stoi(tmp);
            tmp = std::to_string(octal);
        }

        ret += tmp;

        if (syscall == MOUNT && count == 3)
            break;

        if (syscall == SETPROP && count == 1)
            break;

        if (syscall == SERVICE && count == 1) {
            //std::cout << "adding " << ret << std::endl;
            break;
        }

        count++;
    }

    //std::cout << ret << std::endl;

    return ret;
}

// this function takes input from your device's stock ramdisk. each time a file
// is chmod'd or chown'd or written to, we pass it here to see if it's a hardlink.
// we can use the readlink binary to see what the actual location of the file is.
// '/sys/class/leds/button-backlight1/brightness' points to
// '/sys/devices/leds-qpnp-e04fc000/leds/button-backlight1/brightness', so if we
// get a syscall that performs some operation on the latter, use the former because
// it's universal for each and every boot, whereas the latter may only work for this
// boot because its name may be different for the next boot.
void get_symlink_from_file(std::string file) {

    const std::string ADB_START = "adb shell su -c \"readlink -f";
    char buf[256] = {0};
    char *p;
    static int device_plugged = 1;
    std::string cmd;
    FILE *fp;

    if (device_plugged == 0)
        return;

    if (taken_symlinks.find(file) != taken_symlinks.end()) {
        //std::cout << "skip repeat " << file << std::endl;
        return;
    }

    // keep a registry of symlinks mapped to files, to cut down on possible repeats
    // which will save time so we don't have to popen a redundant command
    taken_symlinks.insert(file);

    cmd = ADB_START + " " + file + "\"";

    //std::cout << "file " << file << std::endl;
    //std::cout << cmd << std::endl;

    fp = popen(cmd.c_str(), "r");
    fgets(buf, sizeof(buf), fp);

    // strip newline
    p = strchr(buf, '\n');
    if (p) {
        *p = '\0';
    }

    // FUCK THIS RANDOM FUCKING CARRIAGE RETURN BULLSHIT. I SPENT 3 FUCKING HOURS
    // TRYING TO FIGURE OUT WHY THE FUCK THE STRINGS WEREN'T EQUAL AND IT WAS BECAUSE
    // OF WHATEVER THE FUCK A CARRIAGE RETURN IS BEING TACKED ONTO THE FUCKING STRING.
    // FUCK ASCII CODE 13 FUCK YOU FUCK YOU FUCK YOU FUCK YOU FUCK YOU FUCK YOU
    p = strchr(buf, 13);
    if (p) {
        *p = '\0';
    }

    std::string buf_to_string(buf);

    if (buf_to_string.length() == 0) {
        device_plugged = 0;
        std::cout << "Device not plugged in! Aborting symlink generation." << std::endl;
        pclose(fp);
        sleep(3);
        return;
    }

    // case where the file doesn't exist
    if (buf_to_string.find("readlink:") != std::string::npos) {
        pclose(fp);
        return;
    }

    // if the key has already been mapped
    if (symlink_registry.find(buf_to_string) != symlink_registry.end()) {
        pclose(fp);
        return;
    }

    //std::cout << "file " << file << " buf " << buf_to_string << std::endl;
    symlink_registry[buf_to_string] = file;

    pclose(fp);
}

void get_symlink(std::string in) {

    std::stringstream stream(in);
    std::string tmp;
    std::string link;

    while (stream >> tmp) {
        if (tmp.at(0) == '/') {
            get_symlink_from_file(tmp);
            return;
        }
    }
}

int get_syscall(std::string in, bool upper) {

    int i;
    int len = sizeof(valid_syscalls) / sizeof(valid_syscalls[0]);

    std::string first;
    std::stringstream tok(in);
    tok >> first;

    const std::string *syscall_ptr;

    if (upper)
        syscall_ptr = ii_syscalls;
    else
        syscall_ptr = valid_syscalls;

    for (i = 0; i < len; i++) {
        if (!first.compare(syscall_ptr[i])) {
            //std::cout << "match" << std::endl;
            if (i == ECHO)
                return WRITE;
            return i;
        }
    }

    return -1;
}

void parse_default_init_files(void) {

    std::ifstream input_file;
    std::string line;

    int syscall;

    std::string fixed_string;

    DIR *dir;
    dirent *directory;
    std::string filename;

    dir = opendir(DEFAULT_INIT_FILE_DIR);

    while ((directory = readdir(dir)) != NULL) {

        if (*(directory->d_name) == '.')
            continue;

        if (!strstr(directory->d_name, ".rc") && !strstr(directory->d_name, ".sh")) {
            //std::cout << "skip " << directory->d_name << std::endl;
            continue;
        }

        filename = DEFAULT_INIT_FILE_DIR;
        filename += "/";
        filename += directory->d_name;

        //std::cout << filename << std::endl;

        input_file.open(filename);
        while (std::getline(input_file, line)) {
            syscall = get_syscall(line, false);
            if (syscall < 0)
                continue;
            //std::cout << line << std::endl;
            fixed_string = standardize_string(line, syscall);
            //std::cout << fixed_string << std::endl;

#if 0
            if (default_init_registry.find(fixed_string) != default_init_registry.end())
                std::cout << "     got one " << fixed_string << std::endl;
#endif
            default_init_registry.insert(fixed_string);
            //std::cout << "inserting " << fixed_string << std::endl;
        }
        input_file.close();
    }

    closedir(dir);
}

std::vector<std::string> tokenize_to_vector(std::string in, int syscall) {

    std::vector<std::string> ret;
    std::stringstream token(in);
    std::string tmp;
    int count = 0;
    size_t help;

    while (token >> tmp) {
        ret.push_back(tmp);

        // cannot tokenize a write by spaces because of things like: write /proc/sys/net/ipv4/ping_group_range "0 2147483647"
        // so get up to the file name, then copy the rest to the buffer and break
        if (syscall == WRITE && count == 1) {
            help = token.str().find(tmp);
            tmp = token.str().substr(help + tmp.length() + 1);
            // if we're writing a string or a file, surround in quotes
            if ((tmp.find(" ") != std::string::npos) || !isdigit(tmp.at(0))) {
                tmp = "\"" + tmp + "\"";
            }
            ret.push_back(tmp);
            break;
        }

        count++;
    }

    if (syscall != SERVICE)
        // remove the 0'th element (the syscall is not needed)
        ret.erase(ret.begin());

    return ret;
}

// opens our device's ramdisk files, and looks for the names of services that
// match the input param. if it matches, add the entire service's definition to
// the string, which will be printed out when done.
void generate_services(std::string service_name) {

    std::ifstream input_file;
    std::string line;

    DIR *dir;
    dirent *directory;
    std::string filename;

    std::vector<std::string> service_vector;

    bool found_one = false;

    //std::cout << "name: " << service_name << std::endl;

    dir = opendir(DEVICE_INIT_FILE_DIR);

    while ((directory = readdir(dir)) != NULL) {

        if (*(directory->d_name) == '.')
            continue;

        if (!strstr(directory->d_name, ".rc")) {
            //std::cout << "skip " << directory->d_name << std::endl;
            continue;
        }

        filename = DEVICE_INIT_FILE_DIR;
        filename += "/";
        filename += directory->d_name;

        //std::cout << filename << std::endl;

        input_file.open(filename);
        while (std::getline(input_file, line)) {
            //std::cout << line << std::endl;
            if (line.find(SERVICE_START) != line.npos && line.find(service_name) != line.npos) {
                service_vector = tokenize_to_vector(line, SERVICE);

                // if the line is not exactly service <service_name>, don't go ahead with the printing.
                if (service_vector.at(0).compare(SERVICE_START) != 0 || service_vector.at(1).compare(service_name))
                    continue;

                // if the service is defined in the default init.rc files, skip it; it is not unique to our device.
                if (default_init_registry.find(service_vector.at(0) + " " + service_vector.at(1)) != default_init_registry.end()) {
                    //std::cout << "default " << service_vector.at(0) << " " << service_vector.at(1) << std::endl;
                    input_file.close();
                    closedir(dir);
                    return;
                }

                // if there are multiple definitions of the service, show all of them, with a warning to find which
                // one is the actual correct service definition
                if (found_one)
                    services_list += "# duplicate service definition found; find correct one and remove other(s)!!\n";
                found_one = true;
                //std::cout << line << std::endl;
                services_list += line + "\n";

                while (std::getline(input_file, line)) {
                    //std::cout << "len " << line.length() << std::endl;
                    if (line.length() == 0) {
                        break;
                    }
                    services_list += line += "\n";
                    //std::cout << line << std::endl;
                }
                services_list += "\n";
            }
        }
        input_file.close();
    }

    closedir(dir);
}

// a varaidic function. whether our system call takes 4 params, or just two, it gets
// passed here, and we pop them off and generate a string based on these system calls
// then we check that string against the map generated when we parsed the default init
// files.
void generate_and_check(int syscall, int argc, ...) {

    int i;
    va_list args;
    va_start(args, argc);
    std::string line;

    std::string help;

    line = valid_syscalls[syscall];

    for (i = 0; i < argc; i++) {
        line += " ";
        // if syscall is mkdir or chown, must convert the uid and gid to plain text (not numbers)
        if ((syscall == MKDIR && i >= 2) || (syscall == CHOWN && i <= 1)) {
            line += get_uid_gid(atoi(va_arg(args, char *)));
        } else {
            help = va_arg(args, char *);
            // default perms are 755, so if the perms are indeed 755, don't print them.
            if (syscall == MKDIR && i == argc - 1 && help.compare("755") == 0) {
                // erase the space made from a few lines above
                line.erase(line.length() - 1, 1);
                break;
            }
            line += help;
        }
    }

    // if the system call is NOT a default init system call, print it. (otherwise it's a redundant system call)
    if (default_init_registry.find(line) == default_init_registry.end()) {
        // check to see whether the setprop was even called by init. since setprop is not an actual system call
        // it cannot be intercepted by our kernel changes; so we must use a userspace daemon to notify us if a
        // system property has been changed/added. since we have no possible way of knowing if the uid of the
        // setprop was that of an init process, we must generate map of setprops called by the ramdisk and
        // other scripts, then use that as a whitelist for seeing if the setprop was indeed called by an init
        // process.
        if (syscall == SETPROP && device_setprop_registry.find(line) == device_setprop_registry.end())
            return;

        if (syscall != MOUNT) {
            std::cout << "    " << line;
            if (syscall == SETPROP) {
                std::cout << " ";
                std::cout << va_arg(args, char *);
            }
            std::cout << std::endl;
        } else {
            int i;
            int flag;

            std::string mount_help(va_arg(args, char *));
            std::vector<std::string> mount_args = tokenize_to_vector(mount_help, MOUNT);
            flag = std::stoi(mount_args.at(3));
            // default mount args for when the check flag is present. if they are present, skip.
            // the true mount flags will be in the next few lines parsed.
            if (flag == (MS_NOATIME | MS_NOEXEC | MS_NOSUID)) {
                va_end(args);
                return;
            }
            //std::cout << mount_args.at(0) << std::endl;
            std::cout << "    mount " << mount_args.at(2) + " " + mount_args.at(0) + " " + mount_args.at(1) + " ";
            // << " " << mount_args.at(3) << " " << mount_args.at(4) << std::endl;

            // extract the mount flag information from the integer of the system call param
            for (i = 0; mount_flags[i].name; i++) {
                if (flag & mount_flags[i].flag)
                    std::cout << mount_flags[i].name << " ";
            }

            if (mount_args.at(4).compare("(null)") != 0)
                std::cout << mount_args.at(4);

            std::cout << std::endl;
        }
    }

    va_end(args);
}

// this looks through our symlink map for a match. it returns a universally-known symlink
// rather than the specific location that may be different for each boot.
std::string get_link_mate(std::string file) {

    //std::cout << "got " << file << std::endl;

    std::unordered_map<std::string, std::string>::iterator iter;

    iter = symlink_registry.find(file);

    if (iter == symlink_registry.end()) {
        // init technically chmods and chowns some sockets that have selinux policies, so the program
        // would flag this as a system call, but they aren't ACTUALLY done by the init scripts, and
        // init is gonna do it anyways, so it's unnecessary, so comment it out.
        if (file.find("/dev/socket") != file.npos)
            std::cout << "    #";
        return file;
    }

    //std::cout << "got " << file << std::endl;
    //std::cout << "rec " << symlink_registry.find(file)->second << std::endl;

    return iter->second;
}

// there's a lot of logic going on with mkdir, chmod, and chown. this is because the
// mkdir system call is broken up into a mkdir, a (possible) chmod, then a (possible)
// chown. we must use static flags to pass information as to where we are in the mkdir
// process for each invocation of this function, because:
// MKDIR /persist-lg/property 771
// CHMOD /persist-lg/property 771
// CHOWN /persist-lg/property 1000 1001
// must become 'mkdir /persist-lg/property 771 system radio'
// once we find out that we're done_check function, which will convert the system call
// we just reverse-engineered to a string, and see if the default init.rc does this
// system call. if it does, we don't need it because it's redundant. if it's not done
// in the default init scripts, it's unique to our device, and should be printed out
// because our device will (probably) need that in order to boot or to have a service
// work correctly.
void parse_line(std::string in) {

    int syscall = get_syscall(in, true);

    std::vector<std::string> parts;
    //std::cout << "rec'd " << in << std::endl;

    static enum state current = NOTHING;

    static std::string persist0;
    static std::string persist1;
    static std::string persist2;

    if (in.back() == ' ')
        return;

    if (in.find(II_NEW_SECTION) != std::string::npos) {
        std::cout << std::endl;
        std::cout << "on " << in.substr(in.find(II_NEW_SECTION) + II_NEW_SECTION.length()) << std::endl;
        return;
    }

    parts = tokenize_to_vector(in, syscall);

    // if mkdir is followed by some non-mkdir component instruction, print the persisted mkdir
    if (current != NOTHING && syscall != CHMOD && syscall != CHOWN) {
        if (current == JUST_MKDIR) {
            generate_and_check(MKDIR, 2, persist0.c_str(), persist1.c_str());
        } else {
            generate_and_check(MKDIR, 2, persist0.c_str(), persist2.c_str());
        }
        current = NOTHING;
    }

    switch(syscall) {
        case MKDIR:
            current = JUST_MKDIR;
            persist0 = parts.at(0);
            persist1 = parts.at(1);
            break;
        case CHMOD:
            // if we just did a mkdir, check to see if we're continuing with a chmod
            if (current == JUST_MKDIR) {
                // if the chmod has the same filename, it's still part of the mkdir
                if (persist0.compare(parts.at(0)) == 0) {
                    current = MKDIR_AND_CHMOD;
                    persist2 = parts.at(1);
                } else {
                    // names don't match; two different system calls. process mkdir, then chmod
                    generate_and_check(MKDIR, 2, persist0.c_str(), persist1.c_str());
                    parts.at(0) = get_link_mate(parts.at(0));
                    generate_and_check(CHMOD, 2, parts.at(1).c_str(), parts.at(0).c_str());
                    current = NOTHING;
                }
            } else if (current == MKDIR_AND_CHMOD) {
                    // the possibility that a mkdir and chmod is followed by some chmod
                    generate_and_check(MKDIR, 2, persist0.c_str(), persist2.c_str());
                    parts.at(0) = get_link_mate(parts.at(0));
                    generate_and_check(CHMOD, 2, parts.at(1).c_str(), parts.at(0).c_str());
                    current = NOTHING;
            } else {
                    // they chmod right after a mkdir with a sticky bit, which gives off a false-syscall
                    if (parts.at(0).compare(persist0) == 0 && std::stoi(parts.at(1)) & (S_ISUID | S_ISGID)
                            && parts.at(1).compare(persist1) == 0)
                        return;
                    parts.at(0) = get_link_mate(parts.at(0));
                    generate_and_check(CHMOD, 2, parts.at(1).c_str(), parts.at(0).c_str());
            }
            break;
        case CHOWN:
            // if the chown'd file arg has the same name as the mkdir'd file arg, it's a continuation of mkdir
            // in the form of mkdir /cache/lost+found 0770 root root
            if (current == MKDIR_AND_CHMOD) {
                if (persist0.compare(parts.at(0)) == 0) {
                    generate_and_check(MKDIR, 4, persist0.c_str(), persist2.c_str(), parts.at(1).c_str(), parts.at(2).c_str());
                } else {
                    // chown'd file arg does not match, they are two different system calls, so process the mkdir, then the chown
                    generate_and_check(MKDIR, 2, persist0.c_str(), persist2.c_str());
                    //get_link_mate(parts.at(0));
                    parts.at(0) = get_link_mate(parts.at(0));
                    generate_and_check(CHOWN, 3, parts.at(1).c_str(), parts.at(2).c_str(), parts.at(0).c_str());
                }
                current = NOTHING;
            } else if (current == JUST_MKDIR) {
                if (persist0.compare(parts.at(0)) == 0) {
                    generate_and_check(MKDIR, 4, persist0.c_str(), persist1.c_str(), parts.at(1).c_str(), parts.at(2).c_str());
                } else {
                    generate_and_check(MKDIR, 2, persist0.c_str(), persist1.c_str());
                    parts.at(0) = get_link_mate(parts.at(0));
                    generate_and_check(CHOWN, 3, parts.at(1).c_str(), parts.at(2).c_str(), parts.at(0).c_str());
                }
                current = NOTHING;
            } else {
                // not in the middle of a mkdir syscall, just regular chown
                parts.at(0) = get_link_mate(parts.at(0));
                generate_and_check(CHOWN, 3, parts.at(1).c_str(), parts.at(2).c_str(), parts.at(0).c_str());
            }
            break;
        case WRITE:
                parts.at(0) = get_link_mate(parts.at(0));
                generate_and_check(WRITE, 2, parts.at(0).c_str(), parts.at(1).c_str());
            break;
        case SYMLINK:
                generate_and_check(SYMLINK, 2, parts.at(0).c_str(), parts.at(1).c_str());
            break;
        case MOUNT:
                // we will pass the entire mount argument to the generate_and_check function, even though
                // we say it has only 3 arguments, the 4th will be used to generate the flags.
                generate_and_check(MOUNT, 3, parts.at(2).c_str(), parts.at(0).c_str(), parts.at(1).c_str(), in.c_str());
            break;
        case SETRLIMIT:
                generate_and_check(SETRLIMIT, 3, parts.at(0).c_str(), parts.at(1).c_str(), parts.at(2).c_str());
            break;
        case SETPROP:
                // the setprop case has two purposes. this one here is to see if setprop was called by the default
                // init scripts.
                generate_and_check(SETPROP, 1, parts.at(0).c_str(), parts.at(1).c_str());

                // the other case is that property set was 'init.svc.vold running'. we can use the information
                // that a service has been set to running to add it to our init.<device>.rc file (if the service
                // is not a default one defined in the default init.rc)
                if (parts.at(0).find(II_SERVICE_DECLARED) != parts.at(0).npos && parts.at(1).compare(II_SERVICE_RUNNING) == 0) {
                    //std::cout << parts.at(0) << " " << parts.at(1) << std::endl;
                    generate_services(parts.at(0).substr(II_SERVICE_DECLARED.length()));
                }
                break;
        default:
            std::cout << "??" << std::endl;
    }
}

// this function iterates through your device's default init scripts
// if the bool 'setprop' is true, generates a map of each instance in
// which one of your device's init scripts set a property with setprop.
// since setprop is not a system call, we must use a userspace program
// called prop_watch, and spy on our system properties until one of them
// has changed, then log it in the dmesg buffer. unfortunately, we have
// no way of knowing whether one of the init scripts set the property,
// or if some random binary or library set the program, because setprop
// is not a system call. so we generate a whitelist of times setprop was
// called by an init script; then when we process a setprop, we check it
// against the map generated in this function to see if the setprop was
// indeed called by an init script. if the bool 'setprop' is false, we
// generate a map of files either written, chmod'd, or chown'd by the init
// scripts, and see if they are symlinks. if they are symlinks, generate a
// map of the link and actual location to which the link points to. this is
// done because chown of:
// /sys/devices/leds-qpnp-e04fc000/leds/button-backlight1/brightness
// may not work because of how the virtual filesystem maps directories, but
// generating this map of symlinks will tell us that:
// /sys/class/leds/button-backlight1/brightness points to the actual file
// above, so use the link because that is guarenteed to work for every boot.
void generate_device_registry(bool setprop) {

    std::ifstream input_file;
    std::string line;

    int syscall;

    std::string fixed_string;

    DIR *dir;
    dirent *directory;
    std::string filename;

    dir = opendir(DEVICE_INIT_FILE_DIR);

    while ((directory = readdir(dir)) != NULL) {

        if (*(directory->d_name) == '.')
            continue;

        if (!strstr(directory->d_name, ".rc") && !strstr(directory->d_name, ".sh")) {
            //std::cout << "skip " << directory->d_name << std::endl;
            continue;
        }

        filename = DEVICE_INIT_FILE_DIR;
        filename += "/";
        filename += directory->d_name;

        //std::cout << filename << std::endl;

        input_file.open(filename);
        while (std::getline(input_file, line)) {
            syscall = get_syscall(line, false);
            if (setprop && syscall != SETPROP)
                continue;

            if (syscall < 0)
                continue;

            //std::cout << line << std::endl;
            //std::cout << fixed_string << std::endl;

            if (setprop) {
                fixed_string = standardize_string(line, syscall);
                device_setprop_registry.insert(fixed_string);
            } else if (syscall == CHMOD || syscall == CHOWN || syscall == WRITE) {
                get_symlink(line);
            }

            //std::cout << "inserting " << fixed_string << std::endl;
        }
        input_file.close();
    }

    closedir(dir);
}

void read_ideal_init_file(void) {

    std::ifstream input_file;
    std::string line;

    size_t start;

    input_file.open("JUST_II_DATA.txt");

    while (std::getline(input_file, line)) {
        //std::cout << line << std::endl;
        start = line.find(II_START);
        line = line.substr(start + II_START.length());
        //std::cout << line << std::endl;
        parse_line(line);
    }

    input_file.close();
}

int main(int argc, char **argv) {

    parse_default_init_files();

    generate_device_registry(true);

    std::cout << "Generating symlink table, please wait..." << std::endl;
    generate_device_registry(false);

    read_ideal_init_file();

    std::cout << std::endl;
    std::cout << services_list << std::endl;

    argc = argc;
    argv = argv;

    return 0;
}
