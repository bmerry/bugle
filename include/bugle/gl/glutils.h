/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2009  Bruce Merry
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

#include <stdbool.h>
#include <bugle/gl/glheaders.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <budgie/types.h>
#include <bugle/export.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The intended use is:
 * if (bugle_gl_begin_internal_render())
 * {
 *     CALL(glFrob)();
 *     bugle_gl_end_internal_render();
 * }
 * bugle_gl_begin_internal_render will return false if one is inside begin/end.
 * You must also call bugle_gl_filter_set_renders in the filterset initialiser,
 * as well as bugle_gl_filter_post_renders for each filter that will do rendering
 * after invoke.
 */

BUGLE_EXPORT_PRE bool bugle_gl_begin_internal_render(void) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_end_internal_render(const char *name, bool warn) BUGLE_EXPORT_POST;

/* Registers all commands that trigger geometry (pixel rectangles are
 * not included). This includes commands that use arrays as well as
 * immediate mode commands.
 */
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_drawing(filter *f, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
/* Like the above, but only immediate mode vertex commands */
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_drawing_immediate(filter *f, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;

/* Returns true for glVertex*, and for glVertexAttrib* with attribute
 * 0. These are the calls that generate a vertex in immediate mode.
 * f must be canonicalised.
 */
BUGLE_EXPORT_PRE bool bugle_gl_call_is_immediate(function_call *call) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE void bugle_gl_filter_set_renders(const char *name) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_post_renders(const char *name) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_set_queries_error(const char *name) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE GLenum bugle_gl_call_get_error(object *call_object) BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_SRC_GLUTILS_H */
