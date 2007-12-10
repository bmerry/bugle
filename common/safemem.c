/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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
#include "safemem.h"
#include "linkedlist.h"
#include "threads.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "xalloc.h"

char *bugle_strcat(char *dest, const char *src)
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
        tmp = XNMALLOC(tsize, char);
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

int bugle_appendf(char **strp, size_t *sz, const char *format, ...)
{
#if HAVE_VSNPRINTF
    va_list ap;
    size_t len;       /* of initial portion */
    int ans;

    if (!*strp)
    {
        *sz = 128;
        *strp = XNMALLOC(*sz, char);
        **strp = '\0';
    }
    len = strlen(*strp);
    while (1)
    {
        va_start(ap, format);
        ans = vsnprintf(*strp + len, *sz - len, format, ap);
        va_end(ap);
        if (ans > -1 && ans < *sz - len)    /* Worked */
            return ans + len;

        *sz *= 2;
        if (*sz < ans + 1) *sz = ans + 1;
        *strp = xnrealloc(*strp, *sz, sizeof(char));
    }
#else
#error "you have no safe way to print strings"
#endif
}

char *bugle_afgets(FILE *stream)
{
    char *str = NULL;
    size_t n = 0;
    ssize_t result;

    result = getline(&str, &n, stream);
    if (result <= 0)
    {
        free(str);
        return NULL;
    }
    return str;
}

typedef struct
{
    void (*shutdown_function)(void *);
    void *arg;
} shutdown_call;

static linked_list shutdown_calls;
static bugle_thread_mutex_t shutdown_mutex = BUGLE_THREAD_MUTEX_INITIALIZER;
static bugle_thread_once_t shutdown_once = BUGLE_THREAD_ONCE_INIT;

static void bugle_atexit_handler(void)
{
    linked_list_node *i;
    shutdown_call *call;

    for (i = bugle_list_head(&shutdown_calls); i; i = bugle_list_next(i))
    {
        call = (shutdown_call *) bugle_list_data(i);
        (*call->shutdown_function)(call->arg);
    }
    bugle_list_clear(&shutdown_calls);
}

static void bugle_atexit_once(void)
{
    bugle_list_init(&shutdown_calls, true);
    atexit(bugle_atexit_handler);
}

void bugle_atexit(void (*shutdown_function)(void *), void *arg)
{
    shutdown_call *call;

    call = XMALLOC(shutdown_call);
    call->shutdown_function = shutdown_function;
    call->arg = arg;
    bugle_thread_once(&shutdown_once, bugle_atexit_once);
    bugle_thread_mutex_lock(&shutdown_mutex);
    bugle_list_prepend(&shutdown_calls, call);
    bugle_thread_mutex_unlock(&shutdown_mutex);
}
