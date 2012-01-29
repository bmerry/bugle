/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2008-2009, 2011-2012  Bruce Merry
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

#ifndef BUGLE_GL_GLOBJECTS_H
#define BUGLE_GL_GLOBJECTS_H

#include <bugle/gl/glheaders.h>
#include <bugle/export.h>
#include <bugle/porting.h>

typedef enum
{
    BUGLE_GLOBJECTS_TEXTURE,
    BUGLE_GLOBJECTS_BUFFER,
    BUGLE_GLOBJECTS_VERTEX_ARRAY,
    BUGLE_GLOBJECTS_QUERY,
    BUGLE_GLOBJECTS_SHADER,      /* GLSL */
    BUGLE_GLOBJECTS_PROGRAM,
    BUGLE_GLOBJECTS_OLD_PROGRAM, /* glGenProgramsARB */
    BUGLE_GLOBJECTS_RENDERBUFFER,
    BUGLE_GLOBJECTS_FRAMEBUFFER,
    BUGLE_GLOBJECTS_SYNC,
    BUGLE_GLOBJECTS_TRANSFORM_FEEDBACK,
    BUGLE_GLOBJECTS_COUNT
} bugle_globjects_type;

/* Calls back walker for each object of the specified type. The parameters
 * to the walker are object, target (if available or applicable), and the
 * user data.
 */
BUGLE_EXPORT_PRE void bugle_globjects_walk(bugle_globjects_type type,
                                           void (*walker)(GLuint, GLenum, void *),
                                           void *) BUGLE_EXPORT_POST;

#if BUGLE_GLTYPE_GL
/* Variation of bugle_globjects_walk that deals with sync objects. These
 * can't use the normal interface because the names of sync objects
 * are pointers rather than GLuint.
 */
BUGLE_EXPORT_PRE void bugle_globjects_walk_sync(void (*walker)(GLsync, GLenum, void *),
                                                void *) BUGLE_EXPORT_POST;
#endif

/* Determines the target associated with a particular object, or GL_NONE
 * if none is known.
 */
BUGLE_EXPORT_PRE GLenum bugle_globjects_get_target(bugle_globjects_type type, GLuint id) BUGLE_EXPORT_POST;

/* Used by the initialisation code */
void globjects_initialise(void);

#endif /* !BUGLE_GL_GLOBJECTS_H */
