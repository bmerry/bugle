/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2013  Bruce Merry
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

#include "platform/process.h"
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <bugle/export.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <bugle/log.h>

bugle_bool bugle_process_is_shell(void)
{
    const char *filename = "/proc/self/exe";
    ssize_t result;
    /* readlink(2) suggests using lstat to find the size, but it doesn't work
     * properly on procps. If we overflow, it probably wasn't a shell, since
     * they usually have pretty short paths.
     */
    char target[4096];
    bugle_bool ans = BUGLE_FALSE;

    printf("In bugle_process_is_shell\n");
    result = readlink(filename, target, sizeof(target) - 1); /* make space for \0 */
    printf("result = %d errno = %d\n", (int) result, errno);
    if (result >= 0 && result < sizeof(target) - 1)
    {
        target[result] = '\0'; /* readlink does not terminate it */
        if (strcmp(target, "/bin/sh") == 0
            || strcmp(target, "/bin/ash") == 0
            || strcmp(target, "/bin/bash") == 0
            || strcmp(target, "/bin/csh") == 0
            || strcmp(target, "/bin/dash") == 0
            || strcmp(target, "/bin/ksh") == 0
            || strcmp(target, "/bin/rbash") == 0
            || strcmp(target, "/usr/bin/gdb") == 0
            || strcmp(target, "/usr/bin/screen") == 0)
            ans = BUGLE_TRUE;
        else
        {
            const char *shell = getenv("SHELL");
            if (shell != NULL && strcmp(target, shell) == 0)
                ans = BUGLE_TRUE;
        }
    }
    return ans;
}
