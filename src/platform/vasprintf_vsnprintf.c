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
#include "platform_config.h"
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
    char *ret;
    int len;
    va_list ap2;

    BUGLE_VA_COPY(ap2, ap);

    len = vsnprintf(NULL, 0, format, ap2);
    va_end(ap2);
    ret = BUGLE_NMALLOC(len + 1, char);
    vsnprintf(ret, len, format, ap);
    return ret;
}
