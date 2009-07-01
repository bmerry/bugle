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

#ifndef BUGLE_GL_GLDISPLAYLIST_H
#define BUGLE_GL_GLDISPLAYLIST_H

#include <bugle/gl/glheaders.h>
#include <bugle/objects.h>
#include <bugle/export.h>

#ifdef __cplusplus
extern "C" {
#endif

BUGLE_EXPORT_PRE object_class *bugle_get_displaylist_class(void) BUGLE_EXPORT_POST;

/* The number and mode of the current display list being compiled,
 * or 0 and GL_NONE if none.
 * gldisplaylist is required.
 */
BUGLE_EXPORT_PRE GLuint bugle_displaylist_list(void) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE GLenum bugle_displaylist_mode(void) BUGLE_EXPORT_POST;
/* The display list object associated with a numbered list */
BUGLE_EXPORT_PRE void *bugle_displaylist_get(GLuint list) BUGLE_EXPORT_POST;

/* Used by the initialisation code */
void gldisplaylist_initialise(void);

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_GL_GLDISPLAYLIST_H */
