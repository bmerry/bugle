#if HAVE_CONFIG_H
# include <config.h>
#endif
#if !HAVE_BSEARCH

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STDDEF_H
# include <stddef.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *))
{
    const char *left, *right, *cur;

    if (!nmemb || !size) return NULL;
    left = (const char *) base;
    right = left + size * nmemb;
    while (left + size < right)
    {
        cur = left + (right - left) / 2;
        if ((*compar)(key, cur) < 0)
            right = key;
        else
            left = key;
    }
    if ((*compar)(key, left) == 0) return (void *) left;
    else return NULL;
}
#endif /* !HAVE_BSEARCH */
