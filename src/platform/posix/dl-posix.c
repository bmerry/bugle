/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "platform/dl.h"
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <assert.h>
#include <bugle/string.h>
#include <bugle/log.h>
#include <bugle/memory.h>

void bugle_dl_init(void)
{
    /* Nothing required on POSIX */
}

bugle_dl_module bugle_dl_open(const char *filename, int flag)
{
    char *fullname = NULL;
    void *handle;
    int dlopen_flag = 0;

    assert(filename != NULL);

    if (flag & BUGLE_DL_FLAG_SUFFIX)
    {
        fullname = bugle_asprintf("%s.so", filename);
        filename = fullname;
    }

    if (flag & BUGLE_DL_FLAG_GLOBAL)
        dlopen_flag = RTLD_GLOBAL;
    else
        dlopen_flag = RTLD_LOCAL;

    if (flag & BUGLE_DL_FLAG_NOW)
        dlopen_flag |= RTLD_NOW;
    else
        dlopen_flag |= RTLD_LAZY;
    handle = dlopen(filename, dlopen_flag);

    bugle_free(fullname);
    return handle;
}

int bugle_dl_close(bugle_dl_module module)
{
    assert(module != NULL);
    return dlclose(module);
}

void *bugle_dl_sym_data(bugle_dl_module module, const char *symbol)
{
    assert(module != NULL && symbol != NULL);
    return dlsym(module, symbol);
}

BUDGIEAPIPROC bugle_dl_sym_function(bugle_dl_module module, const char *symbol)
{
    assert(module != NULL && symbol != NULL);
    return (BUDGIEAPIPROC) dlsym(module, symbol);
}

void bugle_dl_foreach(const char *path, void (*callback)(const char *filename, void *arg), void *arg)
{
    DIR *dir;
    struct dirent *item;

    assert(path != NULL);

    dir = opendir(path);
    if (dir == NULL)
    {
        bugle_log_printf("filters", "initialise", BUGLE_LOG_ERROR,
                         "failed to open %s: %s", path, strerror(errno));
        exit(1);
    }

    /* readdir returns NULL for both end-of-stream and error, and
     * leaves errno untouched in the former case.
     */
    errno = 0;
    while (NULL != (item = readdir(dir)))
    {
        size_t len;
        len = strlen(item->d_name);
        if (len >= 3
            && item->d_name[len - 3] == '.'
            && item->d_name[len - 2] == 's'
            && item->d_name[len - 1] == 'o')
        {
            char *filename;

            filename = bugle_asprintf("%s/%s", path, item->d_name);
            callback(filename, arg);
            bugle_free(filename);
        }
    }

    if (errno != 0)
    {
        bugle_log_printf("filters", "initialise", BUGLE_LOG_ERROR,
                         "failed to read from %s: %s", path, strerror(errno));
        exit(1);
    }
}
