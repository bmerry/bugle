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
