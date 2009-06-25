/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2007, 2009  Bruce Merry
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

/* This function overrides dlopen to catch dlopen("libGL.so"), allowing
 * us to intercept even in this case. It would be neater to intercept
 * dlsym and only overload functions that we are interested in catching,
 * but that would leave us no reliable way of getting the original address of
 * dlsym.
 */

#define _GNU_SOURCE /* to get RTLD_NEXT defined */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "platform/dl.h"
#include <dlfcn.h>
#include <string.h>
#include <bugle/bool.h>
#include <bugle/export.h>

#ifdef RTLD_NEXT
# define BUGLE_DEFINED_DLOPEN

static bugle_bool bypass_dlopen;
static void *(*real_dlopen)(const char *, int) = NULL;

void bugle_dl_enable_interception(void)
{
    bypass_dlopen = BUGLE_TRUE;
}

BUGLE_EXPORT_PRE void *dlopen(const char *filename, int flag) BUGLE_EXPORT_POST;
void *dlopen(const char *filename, int flag)
{
    /* TODO-GLES: handle other library names */
    if (bypass_dlopen && filename != NULL)
    {
        if (strcmp(filename, "libGL.so") == 0
            || strcmp(filename, "libGL.so.1") == 0)
        {
            filename = NULL;
            flag = (flag & ~RTLD_GLOBAL) | RTLD_LOCAL;
        }
    }
    if (!real_dlopen)
    {
        /* Note: we do this on demand here rather than unconditionally in
         * constructor, because if neither the application nor bugle is
         * linked to libdl, then dlsym doesn't exist.
         */
        real_dlopen = dlsym(RTLD_NEXT, "dlopen");
    }
    return real_dlopen(filename, flag);
}
#endif /* RTLD_NEXT */

#ifndef BUGLE_DEFINED_DLOPEN
void dlopen_initialise()
{
}
#endif
