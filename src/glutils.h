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

#ifndef BUGLE_SRC_GLUTILS_H
#define BUGLE_SRC_GLUTILS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <GL/glx.h>
#include "common/bool.h"
#include "src/utils.h"
#include <assert.h>

/* The intended use is:
 * if (begin_internal_render())
 * {
 *     CALL_glFrob();
 *     end_internal_render();
 * }
 * begin_internal_render will return false if one is inside begin/end.
 * You must also call filter_set_renders in the filterset initialiser,
 * as well as filter_post_renders for each filter that will do rendering
 * after invoke.
 *
 * If you want to access the context but not necessary do rendering, you
 * can use the weaker _uses_state functions. Note that in_begin_end, as
 * well as ANY OpenGL function call, requires the stronger form.
 */

bool begin_internal_render(void);
void end_internal_render(const char *name, bool warn);
GLXContext get_aux_context();

void register_filter_set_renders(const char *name);
void register_filter_post_renders(const char *name);
void register_filter_set_uses_state(const char *name);
void register_filter_post_uses_state(const char *name);
void register_filter_set_queries_error(const char *name, bool require);
GLenum get_call_error(function_call *call);

bool gl_has_extension(const char *ext);

#endif /* !BUGLE_SRC_GLUTILS_H */
