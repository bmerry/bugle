#ifndef BUGLE_SRC_GLSTATE_H
#define BUGLE_SRC_GLSTATE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "budgielib/state.h"

void glstate_get_enable(state_generic *state);
void glstate_get_global(state_generic *state);
void glstate_get_texparameter(state_generic *state);
void glstate_get_texlevelparameter(state_generic *state);
void glstate_get_texgen(state_generic *state);
void glstate_get_texunit(state_generic *state);
void glstate_get_texenv(state_generic *state);

#endif /* !BUGLE_SRC_GLSTATE_H */
