/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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

#ifndef BUGLE_SRC_GLUTILS_H
#define BUGLE_SRC_GLUTILS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <GL/glx.h>
#include "common/bool.h"
#include "src/utils.h"
#include "src/filters.h"
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
 */

bool bugle_begin_internal_render(void);
void bugle_end_internal_render(const char *name, bool warn);

/* Registers all commands that trigger geometry (pixel rectangles are
 * not included). This includes commands that use arrays as well as
 * immediate mode commands.
 */
void bugle_register_filter_catches_drawing(filter *f, bool inactive, filter_callback callback);
/* Like the above, but only immediate mode vertex commands */
void bugle_register_filter_catches_drawing_immediate(filter *f, bool inactive, filter_callback callback);

/* Returns true for glVertex*, and for glVertexAttrib* with attribute
 * 0. These are the calls that generate a vertex in immediate mode.
 * f must be canonicalised.
 */
bool bugle_call_is_immediate(function_call *call);

void bugle_register_filter_set_renders(const char *name);
void bugle_register_filter_post_renders(const char *name);
void bugle_register_filter_set_queries_error(const char *name);
GLenum bugle_get_call_error(function_call *call);

#endif /* !BUGLE_SRC_GLUTILS_H */
