/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008  Bruce Merry
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

#ifndef BUGLE_GL_GLHEADERS_H
#define BUGLE_GL_GLHEADERS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/porting.h>
#if BUGLE_GLTYPE_GL
# include <GL/gl.h>
# include <GL/glext.h>
# ifndef GL_VERSION_2_0
#  error "Your GL/glext.h is too old! Please obtain the latest version from http://www.opengl.org/registry/"
# endif
#elif BUGLE_GLTYPE_GLES2
# include <GLES2/gl2.h>
# include <GLES2/gl2ext.h>
#elif BUGLE_GLTYPE_GLES1CM
# include <GLES/gl.h>
# include <GLES/glext.h>
#endif

/* To reduce the number of ifdefs in the code, define some tokens that are
 * not universal across all headers
 */
#ifndef GL_NONE
# define GL_NONE 0x0
#endif
#ifndef GL_UNSIGNED_INT
# define GL_UNSIGNED_INT 0x1405
#endif

#endif /* !BUGLE_GL_GLHEADERS_H */
