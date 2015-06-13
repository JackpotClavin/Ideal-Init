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

#ifndef _IDEAL_INIT_H_
#define _IDEAL_INIT_H_

const std::string II_START = "II: ";
const std::string II_NEW_SECTION = "II: ON: ";

const std::string II_SERVICE_DECLARED = "init.svc.";
const std::string II_SERVICE_RUNNING = "running";
const std::string SERVICE_START = "service";

enum state {
    NOTHING = 0,
    JUST_MKDIR,
    MKDIR_AND_CHMOD
};

const std::string valid_syscalls[] = {
    "mkdir",
    "chmod",
    "chown",
    "write",
    "symlink",
    "mount",
    "setrlimit",
    "setprop",
    "echo",
    "service"
};

const std::string ii_syscalls[] = {
    "MKDIR",
    "CHMOD",
    "CHOWN",
    "WRITE",
    "SYMLINK",
    "MOUNT",
    "SETRLIMIT",
    "SETPROP",
    "ECHO",
    "SERVICE",
};

std::string get_uid_gid(int n) {

    switch (n) {
    case 0:
        return "root";
        break;
    case 1000:
        return "system";
        break;
    case 1001:
        return "radio";
        break;
    case 1002:
        return "bluetooth";
        break;
    case 1003:
        return "graphics";
        break;
    case 1004:
        return "input";
        break;
    case 1005:
        return "audio";
        break;
    case 1006:
        return "camera";
        break;
    case 1007:
        return "log";
        break;
    case 1008:
        return "compass";
        break;
    case 1009:
        return "mount";
        break;
    case 1010:
        return "wifi";
        break;
    case 1011:
        return "adb";
        break;
    case 1012:
        return "install";
        break;
    case 1013:
        return "media";
        break;
    case 1014:
        return "dhcp";
        break;
    case 1015:
        return "sdcard_rw";
        break;
    case 1016:
        return "vpn";
        break;
    case 1017:
        return "keystore";
        break;
    case 1018:
        return "usb";
        break;
    case 1019:
        return "drm";
        break;
    case 1020:
        return "mdnsr";
        break;
    case 1021:
        return "gps";
        break;
    case 1023:
        return "media_rw";
        break;
    case 1024:
        return "mtp";
        break;
    case 1026:
        return "drmrpc";
        break;
    case 1027:
        return "nfc";
        break;
    case 1028:
        return "sdcard_r";
        break;
    case 1029:
        return "clat";
        break;
    case 1030:
        return "loop_radio";
        break;
    case 1031:
        return "mediadrm";
        break;
    case 1032:
        return "package_info";
        break;
    case 1033:
        return "sdcard_pics";
        break;
    case 1034:
        return "sdcard_av";
        break;
    case 1035:
        return "sdcard_all";
        break;
    case 1036:
        return "logd";
        break;
    case 1037:
        return "shared_relro";
        break;
    case 1049:
        return "audit";
        break;
    case 2000:
        return "shell";
        break;
    case 2001:
        return "cache";
        break;
    case 2002:
        return "diag";
        break;
    case 3001:
        return "net_bt_admin";
        break;
    case 3002:
        return "net_bt";
        break;
    case 3003:
        return "inet";
        break;
    case 3004:
        return "net_raw";
        break;
    case 3005:
        return "net_admin";
        break;
    case 3006:
        return "net_bw_stats";
        break;
    case 3010:
        return "ims";
        break;
    case 3007:
        return "net_bw_acct";
        break;
    case 3008:
        return "net_bt_stack";
        break;
    case 3009:
        return "qcom_diag";
        break;
    case 3011:
        return "sensors";
        break;
    case 3012:
        return "rfs";
        break;
    case 3013:
        return "rfs_shared";
        break;
    case 9997:
        return "everybody";
        break;
    case 9998:
        return "misc";
        break;
    case 9999:
        return "nobody";
        break;
    }

    return "UNKNOWN_UID/GID";
}

/*
 * These are the fs-independent mount-flags: up to 32 flags are supported
 */
#define MS_RDONLY	 1	/* Mount read-only */
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
#define MS_NODEV	 4	/* Disallow access to device special files */
#define MS_NOEXEC	 8	/* Disallow program execution */
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
#define MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
#define MS_DIRSYNC	128	/* Directory modifications are synchronous */
#define MS_NOATIME	1024	/* Do not update access times. */
#define MS_NODIRATIME	2048	/* Do not update directory access times */
#define MS_BIND		4096
#define MS_MOVE		8192
#define MS_REC		16384
#define MS_VERBOSE	32768	/* War is peace. Verbosity is silence.
				   MS_VERBOSE is deprecated. */
#define MS_SILENT	32768
#define MS_POSIXACL	(1<<16)	/* VFS does not apply the umask */
#define MS_UNBINDABLE	(1<<17)	/* change to unbindable */
#define MS_PRIVATE	(1<<18)	/* change to private */
#define MS_SLAVE	(1<<19)	/* change to slave */
#define MS_SHARED	(1<<20)	/* change to shared */
#define MS_RELATIME	(1<<21)	/* Update atime relative to mtime/ctime. */
#define MS_KERNMOUNT	(1<<22) /* this is a kern_mount call */
#define MS_I_VERSION	(1<<23) /* Update inode I_version field */
#define MS_STRICTATIME	(1<<24) /* Always perform atime updates */
#define MS_NOSEC	(1<<28)
#define MS_BORN		(1<<29)
#define MS_ACTIVE	(1<<30)
#define MS_NOUSER	(1<<31)

static struct {
    const char *name;
    unsigned flag;
} mount_flags[] = {
    { "noatime",    MS_NOATIME },
    { "noexec",     MS_NOEXEC },
    { "nosuid",     MS_NOSUID },
    { "nodev",      MS_NODEV },
    { "nodiratime", MS_NODIRATIME },
    { "ro",         MS_RDONLY },
    { "rw",         0 },
    { "remount",    MS_REMOUNT },
    { "bind",       MS_BIND },
    { "rec",        MS_REC },
    { "unbindable", MS_UNBINDABLE },
    { "private",    MS_PRIVATE },
    { "slave",      MS_SLAVE },
    { "shared",     MS_SHARED },
    { "defaults",   0 },
    { 0,            0 },
};

#endif
