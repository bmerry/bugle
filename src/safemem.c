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
#include "safemem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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