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
    ptr = xrealloc(ptr, size);
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
