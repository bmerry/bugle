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
#if HAVE_VASPRINTF
    char *ret = NULL;
    vasprintf(&ret, format, ap);
    if (ret == NULL)
        bugle_alloc_die();
    return ret;
#else /* !HAVE_VASPRINTF */
    char *ret;
    int len;
    va_list ap2;

#if HAVE_VA_COPY
    va_copy(ap2, ap);
#else
    memcpy(ap2, ap, sizeof(ap));
#endif

    len = vsnprintf(NULL, 0, format, ap2);
    va_end(ap2);
    ret = BUGLE_NMALLOC(len + 1, char);
    vsnprintf(ret, len, format, ap);
    return ret;
#endif
}

char *bugle_strdup(const char *str)
{
#if HAVE_STRDUP
    char *ret;

    ret = strdup(str);
    if (ret == NULL) bugle_alloc_die();
    return ret;
#else
    char *ret;
    size_t len;

    len = strlen(str);
    ret = BUGLE_NMALLOC(len + 1, char);
    strcpy(ret, str);
    return ret;
#endif
}

char *bugle_strndup(const char *str, size_t n)
{
#if HAVE_STRNDUP
    char *ret;

    ret = strndup(str, n);
    if (ret == NULL) bugle_alloc_die();
    return ret;
#else
    char *ret;
    size_t len = 0;

    while (len < n && str[len] != '\0') len++;
    ret = BUGLE_NMALLOC(len + 1, char);
    memcpy(ret, str, len * sizeof(char));
    ret[len] = '\0';
    return ret;
#endif
}
