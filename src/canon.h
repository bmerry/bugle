#ifndef BUGLE_SRC_CANON_H
#define BUGLE_SRC_CANON_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/types.h"

void initialise_canonical(void);
budgie_function canonical_call(const function_call *call);

#endif /* !BUGLE_SRC_CANON_H */
