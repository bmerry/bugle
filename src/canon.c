#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "canon.h"
#include "src/types.h"
#include "common/bool.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if !HAVE_BSEARCH
void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *));
#endif

static budgie_function canonical_table[NUMBER_OF_FUNCTIONS];

static int compare_functions_by_name(const void *a, const void *b)
{
    return strcmp((*(const function_data * const *) a)->name,
                  (*(const function_data * const *) b)->name);
}

/* Like strncmp, but the full length of b is considered */
static int strhalfncmp(const char *a, const char *b, size_t n)
{
    int test;

    test = strncmp(a, b, n);
    if (test) return test;
    if (strlen(b) > n) return -1;
    return 0;
}

void initialise_canonical(void)
{
    /* function data sorted by name */
    const function_data *function_names[NUMBER_OF_FUNCTIONS];
    const function_data **alias;
    const void *key;
    budgie_function i;
    const char *name, *end;

    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        function_names[i] = &function_table[i];
    qsort(function_names, NUMBER_OF_FUNCTIONS, sizeof(const function_data *),
          compare_functions_by_name);

    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
    {
        canonical_table[i] = i; /* assume that this is canonical */
        name = function_table[i].name;
        if (strncmp(name, "gl", 2) != 0) continue; /* not a GL/GLX function */
        end = name + strlen(name) - 1;
        key = &function_table[i];
        alias = (const function_data **) bsearch(&key, function_names,
                                                 NUMBER_OF_FUNCTIONS, sizeof(const function_data *),
                                                 compare_functions_by_name);
        /* Extension suffices are sequences of upper-case characters, but
         * may not be maximal (e.g. glGetCompressedTexImage2DARB).
         * Note: don't use isupper here, since we DON'T want to
         * use locale-specific versions. */
        assert(alias != NULL);
        while (*end >= 'A' && *end <= 'Z')
        {
            while (alias >= function_names
                   && strhalfncmp(name, (*alias)->name, end - name) < 0)
                alias--;
            if (strhalfncmp(name, (*alias)->name, end - name) == 0)
            {
                canonical_table[i] = (*alias) - function_table;
                fprintf(stderr, "canonicalising %s to %s\n",
                        name, function_table[canonical_table[i]].name);
                break;
            }
            end--;
        }
    }
}

budgie_function canonical_call(const function_call *call)
{
    if (call->generic.id < 0 || call->generic.id >= NUMBER_OF_FUNCTIONS)
        return call->generic.id;
    else
        return canonical_table[call->generic.id];
}
