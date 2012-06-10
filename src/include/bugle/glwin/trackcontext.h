/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2008-2009  Bruce Merry
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

#ifndef BUGLE_TRACKCONTEXT_H
#define BUGLE_TRACKCONTEXT_H

#include <stddef.h>
#include <bugle/bool.h>
#include <bugle/glwin/glwintypes.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/export.h>

#ifdef __cplusplus
extern "C" {
#endif

struct glwin_context_create;

BUGLE_EXPORT_PRE object_class *bugle_get_context_class(void) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE object_class *bugle_get_namespace_class(void) BUGLE_EXPORT_POST;

/* Gets a context with the same config. Please leave all state as default.
 * There is a choice of a shared or an unshared context, which refers to
 * whether object state is shared with the primary context. The former is
 * best for querying object state, the latter for creating internal objects
 * so as not to pollute the primary namespace.
 */
BUGLE_EXPORT_PRE glwin_context bugle_get_aux_context(bugle_bool shared) BUGLE_EXPORT_POST;

/* Register a newly created context with information about it should be
 * duplicated. This is not normally needed, but can be used by filtersets that
 * override context creation function calls. It is also used by the
 * "trackcontext" filterset internally.
 *
 * If multiple such calls are made for the same context, the first applies.
 * Thus, a filterset wishing to override context creation should ensure that
 * it runs before trackcontext.
 *
 * The create argument is consumed and possibly destroyed. The caller should
 * not attempt to free it, nor refer to it again.
 *
 * The return value is true on success, false if the context was already
 * registered.
 */
BUGLE_EXPORT_PRE bugle_bool bugle_glwin_newcontext(struct glwin_context_create *create) BUGLE_EXPORT_POST;

/* Draws text at the specified location. The current colour is used to
 * render the text. The text is rendered with alpha, so alpha-test or
 * alpha-blending can be used to obtain a transparent background.
 * Pre-conditions:
 * - the unshared aux context is active
 * - the projection matrix is the identity
 * - the modelview matrix is the identity, scaled and optionally translated
 *   such that one unit is one pixel
 * - the pixel unpack state is the default
 * - no textures are enabled
 * - must be inside begin_internal_render/end_internal_render
 * - there must be room for one push on the attribute stack
 */
BUGLE_EXPORT_PRE void bugle_gl_text_render(const char *msg, int x, int y) BUGLE_EXPORT_POST;

/* Used by the initialisation code */
void trackcontext_initialise(void);

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_TRACKCONTEXT_H */
