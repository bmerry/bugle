#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "canon.h"
#include "src/types.h"

/* Canonicalisation; not actually a filter-set */

static budgie_function canonical_table[NUMBER_OF_FUNCTIONS];

void initialise_canonical(void)
{
    /* FIXME: do properly */
    budgie_function i;
    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        canonical_table[i] = i;
}

budgie_function canonical_call(const function_call *call)
{
    if (call->generic.id < 0 || call->generic.id >= NUMBER_OF_FUNCTIONS)
        return call->generic.id;
    else
        return canonical_table[call->generic.id];
}
