/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009-2010  Bruce Merry
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
#include <bugle/string.h>
#include <bugle/memory.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "platform/macros.h"

char *bugle_asprintf(const char *format, ...)
{
    va_list ap;
    char *ret;

    va_start(ap, format);
    ret = bugle_vasprintf(format, ap);
    va_end(ap);
    return ret;
}

char *bugle_vasprintf(const char *format, va_list ap)
{
    /* MSVC's vsnprintf fails to be C99-compliant in several ways:
     * - on overflow, returns -1 instead of string length
     * - on overflow, completely fills the buffer without a \0
     * vsnprintf_s with _TRUNCATE fixes the latter but not the former.
     *
     * Note that MinGW provides a working replacement.
     */

    char *buffer;
    size_t size;
    int len;
    va_list ap2;

    size = 16;
    buffer = BUGLE_NMALLOC(size, char);
    while (1)
    {
        BUGLE_VA_COPY(ap2, ap);
        len = vsnprintf_s(buffer, size, _TRUNCATE, format, ap2);
        va_end(ap2);
        if (len >= 0)
        {
            /* We are expected to have advanced ap over the args */
            vsnprintf_s(buffer, size, _TRUNCATE, format, ap);
            return buffer;
        }
        size *= 2;
        buffer = BUGLE_NREALLOC(buffer, size, char);
    }
}

