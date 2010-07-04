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
#include "platform_config.h"
#include <bugle/string.h>
#include <bugle/memory.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

int bugle_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int bugle_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf(str, size, format, ap);
}
