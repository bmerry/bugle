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

#ifndef BUGLE_SRC_TRACKER_H
#define BUGLE_SRC_TRACKER_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include "common/bool.h"
#include "src/utils.h"
#include "src/filters.h"
#include "src/objects.h"

extern object_class context_class, displaylist_class;

/* Returns the current context wrapper, or NULL if no current context.
 * trackcontext is required.
 */
state_7context_I *tracker_get_context_state(void);

/* True if we are in begin/end, OR if there is no current context.
 * trackbeginend is required.
 */
bool in_begin_end(void);

/* The number and mode of the current display list being compiled,
 * or 0 and GL_NONE if none.
 * trackdisplaylist is required.
 */
GLuint displaylist_list(void);
GLenum displaylist_mode(void);
/* The display list object associated with a numbered list */
void *displaylist_get(GLuint list);

/* Used by the initialisation code */
void trackcontext_initialise(void);
void trackbeginend_initialise(void);
void trackdisplaylist_initialise(void);

#endif /* !BUGLE_SRC_TRACKER_H */
