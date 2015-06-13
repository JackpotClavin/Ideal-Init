/*
 * Copyright (c) 2008, The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *  * Neither the name of Google, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#include <sys/resource.h>

#include <cutils/properties.h>
#include <cutils/hashmap.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#define KMSG_LOG "/dev/kmsg"

static int str_hash(void *key)
{
    return hashmapHash(key, strlen(key));
}

static bool str_equals(void *keyA, void *keyB)
{
    return strcmp(keyA, keyB) == 0;
}

static void announce(char *name, char *value)
{
    unsigned char *x;
    FILE *fp;

    fp = fopen(KMSG_LOG, "a+");
    if (!fp)
        return;
    
    for(x = (unsigned char *)value; *x; x++) {
        if((*x < 32) || (*x > 127)) *x = '.';
    }

    fprintf(fp, "II: SETPROP %s %s\n", name, value);

    fclose(fp);
}

static void add_to_watchlist(Hashmap *watchlist, const char *name,
        const prop_info *pi)
{
    char *key = strdup(name);
    unsigned *value = malloc(sizeof(unsigned));
    if (!key || !value)
        exit(1);

    *value = __system_property_serial(pi);
    hashmapPut(watchlist, key, value);
}

static void populate_watchlist(const prop_info *pi, void *cookie)
{
    Hashmap *watchlist = cookie;
    char name[PROP_NAME_MAX];
    char value_unused[PROP_VALUE_MAX];

    __system_property_read(pi, name, value_unused);
    add_to_watchlist(watchlist, name, pi);
}

static void update_watchlist(const prop_info *pi, void *cookie)
{
    Hashmap *watchlist = cookie;
    char name[PROP_NAME_MAX];
    char value[PROP_VALUE_MAX];
    unsigned *serial;

    __system_property_read(pi, name, value);
    serial = hashmapGet(watchlist, name);
    if (!serial) {
        add_to_watchlist(watchlist, name, pi);
        announce(name, value);
    } else {
        unsigned tmp = __system_property_serial(pi);
        if (*serial != tmp) {
            *serial = tmp;
            announce(name, value);
        }
    }
}

int main(int argc, char *argv[])
{
    unsigned serial;
    int which = PRIO_PROCESS;
    pid_t pid;
    int priority = -20;

    pid = getpid();
    setpriority(which, pid, priority);
    
    Hashmap *watchlist = hashmapCreate(1024, str_hash, str_equals);
    if (!watchlist)
        exit(1);

    __system_property_foreach(populate_watchlist, watchlist);

    for(serial = 0;;) {
        serial = __system_property_wait_any(serial);
        __system_property_foreach(update_watchlist, watchlist);
    }

    argc = argc;
    argv = argv;
    return 0;
}
