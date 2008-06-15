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

#ifndef BUGLE_GL_GLOBJECTS_H
#define BUGLE_GL_GLOBJECTS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>

typedef enum
{
    BUGLE_GLOBJECTS_TEXTURE,
    BUGLE_GLOBJECTS_BUFFER,
    BUGLE_GLOBJECTS_QUERY,
    BUGLE_GLOBJECTS_SHADER,      /* GLSL */
    BUGLE_GLOBJECTS_PROGRAM,
    BUGLE_GLOBJECTS_OLD_PROGRAM, /* glGenProgramsARB */
    BUGLE_GLOBJECTS_RENDERBUFFER,
    BUGLE_GLOBJECTS_FRAMEBUFFER,
    BUGLE_GLOBJECTS_COUNT
} bugle_globjects_type;

/* Calls back walker for each object of the specified type. The parameters
 * to the walker are object, target (if available or applicable), and the
 * user data.
 */
void bugle_globjects_walk(bugle_globjects_type type,
                             void (*walker)(GLuint, GLenum, void *),
                             void *);
/* Determines the target associated with a particular object, or GL_NONE
 * if none is known.
 */
GLenum bugle_globjects_get_target(bugle_globjects_type type, GLuint id);

/* Used by the initialisation code */
void globjects_initialise(void);

#endif /* !BUGLE_GL_GLOBJECTS_H */
