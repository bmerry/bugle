/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
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

#ifndef BUGLE_SRC_GLDUMP_H
#define BUGLE_SRC_GLDUMP_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <GL/glx.h>
#include "budgieutils.h"
#include "common/bool.h"
#include "src/gltokens.h"

GLenum bugle_gl_token_to_enum(const char *name);
const gl_token *bugle_gl_enum_to_token_struct(GLenum e);
const char *bugle_gl_enum_to_token(GLenum e);

budgie_type bugle_gl_type_to_type(GLenum gl_type);
budgie_type bugle_gl_type_to_type_ptr(GLenum gl_type);
size_t bugle_gl_type_to_size(GLenum gl_type);
int bugle_gl_format_to_count(GLenum format, GLenum type);

/* Initialiser for certain dumping function */
void initialise_dump_tables(void);

int bugle_count_gl(budgie_function func, GLenum token);
int bugle_count_global_query(GLenum token);
bool bugle_dump_GLenum(GLenum e, FILE *out);
bool bugle_dump_GLalternateenum(GLenum e, FILE *out);
bool bugle_dump_GLcomponentsenum(GLenum e, FILE *out);
bool bugle_dump_GLerror(GLenum e, FILE *out);
bool bugle_dump_GLboolean(GLboolean b, FILE *out);
bool bugle_dump_GLXDrawable(GLXDrawable d, FILE *out);
bool bugle_dump_GLpolygonstipple(const GLubyte (*pattern)[4], FILE *out);
bool bugle_dump_convert(GLenum pname, const void *value,
                        budgie_type in_type, FILE *out);

/* Computes the number of pixel elements (units of byte, int, float etc)
 * used by a client-side encoding of a 1D, 2D or 3D image.
 * Specify -1 for depth for 1D or 2D textures.
 */
size_t bugle_image_element_count(GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type,
                                 bool unpack);

/* Computes the number of pixel elements required by glGetTexImage
 */
size_t bugle_texture_element_count(GLenum target,
                                   GLint level,
                                   GLenum format,
                                   GLenum type);

#endif /* !BUGLE_SRC_GLDUMP_H */
