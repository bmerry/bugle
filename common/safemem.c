/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#define _GNU_SOURCE
#include "safemem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

void *xmalloc(size_t size)
{
    void *ptr;
    if (size == 0) size = 1;
    ptr = malloc(size);
    if (!ptr)
    {
        fputs("out of memory in malloc\n", stderr);
        exit(1);
    }
    return ptr;
}

void *xcalloc(size_t nmemb, size_t size)
{
    void *ptr;
    ptr = calloc(nmemb, size);
    if (!ptr)
    {
        fputs("out of memory in calloc\n", stderr);
        exit(1);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (size && !ptr)
    {
        fputs("out of memory in realloc\n", stderr);
        exit(1);
    }
    return ptr;
}

char *xstrdup(const char *s)
{
    /* strdup is not ANSI or POSIX, so we reimplement it */
    char *n;
    size_t len;

    len = strlen(s);
    n = xmalloc(len + 1);
    memcpy(n, s, len + 1);
    return n;
}

char *xstrcat(char *dest, const char *src)
{
    size_t dlen, dsize, slen, tsize;
    char *tmp;

    slen = strlen(src);
    dlen = strlen(dest);
    dsize = 1;
    while (dsize <= dlen) dsize *= 2;
    tsize = dsize;
    if (tsize <= slen + dlen)
    {
        while (tsize <= slen + dlen) tsize *= 2;
        tmp = xmalloc(tsize);
        memcpy(tmp, dest, dlen);
        memcpy(tmp + dlen, src, slen + 1);
        free(dest);
        return tmp;
    }
    else
    {
        memcpy(dest + dlen, src, slen + 1);
        return dest;
    }
}

int xasprintf(char **strp, const char *format, ...)
{
    int ans;
#if !HAVE_VASPRINTF && HAVE_VSNPRINTF
    size;
#endif

    va_list ap;
#if HAVE_VASPRINTF
    va_start(ap, format);
    ans = vasprintf(strp, format, ap);
    if (!*strp)
    {
        fputs("out of memory in vasprintf\n", stderr);
        exit(1);
    }
    va_end(ap);
    return ans;
#elif HAVE_VSNPRINTF
    size = 128;
    *strp = xmalloc(size);
    while (1)
    {
        va_start(ap, format);
        ans = vsnprintf(*strp, size, format, ap);
        va_end(ap);
        if (ans > -1 && n < size) /* Worked */
            return ans;
        else if (ans > -1)        /* C99 standard: number of bytes */
            size = ans + 1;
        else                      /* Older e.g. glibc 2.0 */
            size *= 2;
        *strp = xrealloc(*strp, size);
    }
#else
#error "you have no safe way to format strings"
#endif
}

char *xafgets(FILE *stream)
{
    char *str, *ret;
    int size, have;

    size = 16;
    have = 0;
    str = xmalloc(size);
    while (1)
    {
        ret = fgets(str + have, size - have, stream);
        if (!ret)
        {
            /* Error or EOF before anything new written */
            if (have == 0)
            {
                free(str);
                return NULL;
            }
            else
            {
                str[have] = '\0'; /* just to be safe; fgets might do this */
                return str;
            }
        }
        have += strlen(str + have);
        /* We can (and must) terminate in two cases: a short read, or a
         * full read with a newline at the end. */
        if (have < size - 1 || str[size - 2] == '\n')
            return str;
        size *= 2;
        str = xrealloc(str, size);
    }
}