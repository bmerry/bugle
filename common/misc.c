/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007, 2009  Bruce Merry
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
#define _BSD_SOURCE /* For finite() */
#define _GNU_SOURCE /* For getline() (gnulib provides on non-GNU systems) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>
#include <bugle/misc.h>
#include <bugle/string.h>
#include <bugle/memory.h>
#include <bugle/linkedlist.h>
#include "threads.h"

char *bugle_string_io(void (*call)(char **, size_t *, void *), void *data)
{
    char *buffer = NULL, *ptr;
    size_t len = 0;

    call(&buffer, &len, data);
    len = buffer - (char *) NULL + 1;
    buffer = BUGLE_NMALLOC(len, char);
    ptr = buffer;
    call(&ptr, &len, data);
    return buffer;
}

int bugle_appendf(char **strp, size_t *sz, const char *format, ...)
{
    va_list ap;
    size_t len;       /* of initial portion */
    ssize_t ans;

    if (!*strp)
    {
        *sz = 128;
        *strp = BUGLE_NMALLOC(*sz, char);
        **strp = '\0';
    }
    len = strlen(*strp);

    va_start(ap, format);
    ans = bugle_vsnprintf(*strp + len, *sz - len, format, ap);
    va_end(ap);

    /* C99 says that the return value is the number of characters that
     * *would* have been written, if not for truncation.
     */
    if (ans < 0)
        return 0;  /* Output error */
    else if (ans >= (ssize_t) (*sz - len))
    {
        /* Ensure we at least double, to avoid O(N^2) algorithms */
        *sz *= 2;
        if (ans >= (ssize_t) (*sz - len))
            *sz = ans + len + 1;
        *strp = bugle_nrealloc(*strp, *sz, sizeof(char));
        va_start(ap, format);
        ans = bugle_vsnprintf(*strp + len, *sz - len, format, ap);
        va_end(ap);
    }
    if (ans < 0) return ans;
    else return ans + len;
}
