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
#include <bugle/string.h>
#include <bugle/memory.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "platform/macros.h"

int bugle_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = bugle_vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int bugle_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    /* MSVC's vsnprintf fails to be C99-compliant in several ways:
     * - on overflow, returns -1 instead of string length
     * - on overflow, completely fills the buffer without a \0
     * vsnprintf_s with _TRUNCATE fixes the latter but not the former.
     *
     * We try once, if it doesn't fit we have no choice but to actually
     * allocate a big enough buffer just to find out how big the string is
     * (which in turn works by successive doubling...)
     *
     * Note that MinGW provides a C99-compliant replacement.
     */
    va_list ap2;
    int ret;
    char *buffer = NULL;

    BUGLE_VA_COPY(ap2, ap);
    ret = vsnprintf_s(str, size, _TRUNCATE, format, ap);

    if (ret < 0)
    {
        buffer = bugle_vasprintf(format, ap2);
        ret = strlen(buffer);
        bugle_free(buffer);
    }
    va_end(ap2);
    return ret;
}
