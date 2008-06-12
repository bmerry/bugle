/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2008  Bruce Merry
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include <stdbool.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/glwin.h>
#include <budgie/macros.h>

extern object_class *bugle_context_class, *bugle_namespace_class;

/* Gets a context with the same config. Please leave all state as default.
 * There is a choice of a shared or an unshared context, which refers to
 * whether object state is shared with the primary context. The former is
 * best for querying object state, the latter for creating internal objects
 * so as not to pollute the primary namespace.
 */
bugle_glwin_context bugle_get_aux_context(bool shared);

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
void bugle_text_render(const char *msg, int x, int y);

/* Used by the initialisation code */
void trackcontext_initialise(void);

#endif /* !BUGLE_TRACKCONTEXT_H */
