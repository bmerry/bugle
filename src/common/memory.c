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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#include <bugle/memory.h>

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

void *bugle_malloc(size_t size)
{
    void *ret;
    if (size == 0) size = 1;
    ret = malloc(size);
    if (ret == NULL) bugle_alloc_die();
    return ret;
}

void *bugle_calloc(size_t nmemb, size_t size)
{
    if (nmemb > SIZE_MAX / size)
        bugle_alloc_die();
    return bugle_zalloc(nmemb * size);
}

void *bugle_nmalloc(size_t nmemb, size_t size)
{
    if (nmemb > SIZE_MAX / size)
        bugle_alloc_die();
    return bugle_malloc(nmemb * size);
}

void *bugle_zalloc(size_t size)
{
    void *ret;
    ret = bugle_malloc(size);
    memset(ret, 0, size);
    return ret;
}

void *bugle_nrealloc(void *ptr, size_t nmemb, size_t size)
{
    void *ret;
    if (nmemb > SIZE_MAX / size)
        bugle_alloc_die();
    ret = realloc(ptr, nmemb * size);
    if (ret == NULL)
        bugle_alloc_die();
    return ret;
}

static bugle_alloc_die_callback die_callback = NULL;

bugle_alloc_die_callback bugle_get_alloc_die(void)
{
    return die_callback;
}

void bugle_set_alloc_die(bugle_alloc_die_callback callback)
{
    die_callback = callback;
}

void bugle_alloc_die(void)
{
    bugle_alloc_die_callback callback = bugle_get_alloc_die();

    if (callback != NULL)
        callback();
    else
        fprintf(stderr, "bugle: memory allocation failed\n");
    abort();
}

void bugle_free(void *ptr)
{
    free(ptr);
}
