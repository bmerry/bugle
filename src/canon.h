#ifndef BUGLE_SRC_CANON_H
#define BUGLE_SRC_CANON_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/types.h"
#include "budgielib/budgieutils.h"

void initialise_canonical(void);

budgie_function canonical_function(budgie_function f);
budgie_function canonical_call(const function_call *call);
budgie_function find_function(const char *name);

#endif /* !BUGLE_SRC_CANON_H */
