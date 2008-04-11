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

#ifndef BUGLE_SRC_GLREFLECT_H
#define BUGLE_SRC_GLREFLECT_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>
#include <GL/gl.h>
#include <budgie/types.h>

/*** Extensions ***/
typedef int bugle_gl_extension;
#define NULL_EXTENSION (-1)

/* Checks for GL extensions by #define from glexts.h or glxexts.h */
int                 bugle_gl_extension_count(void);
const char *        bugle_gl_extension_name(bugle_gl_extension ext);
/* The GL/GLX core version represented, or NULL for actual extensions */
const char *        bugle_gl_extension_version(bugle_gl_extension ext);
bool                bugle_gl_extension_is_glwin(bugle_gl_extension ext);
bugle_gl_extension  bugle_gl_extension_id(const char *name);

/* Returns a pointer to a list terminated by NULL_EXTENSION. The storage is
 * static, so do not try to free it.
 */
const bugle_gl_extension *bugle_gl_extension_group_members(bugle_gl_extension ext);

/*** Tokens ***/

const char *bugle_gl_enum_name(GLenum e);
/* Returns a list of extensions that define the token,
 * terminated by NULL_EXTENSION. This includes GL_VERSION_x_y "extensions".
 */
const bugle_gl_extension *bugle_gl_enum_extensions(GLenum e);

/*** Functions ***/

bugle_gl_extension  bugle_gl_function_extension(budgie_function id);

#endif /* BUGLE_SRC_GLREFLECT_H */
