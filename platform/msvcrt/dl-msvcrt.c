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

#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <bugle/string.h>
#include <bugle/log.h>
#include <io.h>

bugle_dl_module bugle_dl_open(const char *filename, int flag)
{
    char *fullname = NULL;
    void *handle;
    int dlopen_flag = 0;

    assert(filename != NULL);

    if (flag & BUGLE_DL_FLAG_SUFFIX)
    {
        fullname = bugle_asprintf("%s.dll", filename);
        filename = fullname;
    }

    handle = (void *) LoadLibrary(filename);

    free(fullname);
    return handle;
}

int bugle_dl_close(bugle_dl_module module)
{
    assert(module != NULL);
    return FreeLibrary(module) ? 0 : -1;
}

void *bugle_dl_sym_data(bugle_dl_module module, const char *symbol)
{
    /* Windows doesn't appear to support a data version of GetProcAddress */
    assert(0);
}

BUDGIEAPIPROC bugle_dl_sym_function(bugle_dl_module module, const char *symbol)
{
    assert(module != NULL && symbol != NULL);
    return GetProcAddress(module, symbol);
}

void bugle_dl_foreach(const char *path, void (*callback)(const char *filename, void *arg), void *arg)
{
    struct dirent *item;
    intptr_t handle;
    char *spec;
    struct _finddata_t data;

    assert(path != NULL);

    spec = bugle_asprintf("%s\\*.dll", path);
    handle = _findfirst(spec, &data);
    free(spec);
    if (handle == 0)
    {
        bugle_log_printf("filters", "initialise", BUGLE_LOG_ERROR,
                         "did not find any filter-set libraries in %s", path);
        exit(1);
    }

    do
    {
        char *filename;

        filename = bugle_asprintf("%s/%s", path, data.name);
        callback(filename, arg);
        free(filename);
    } while (_findnext(handle, &data));

    _findclose(handle);
}
