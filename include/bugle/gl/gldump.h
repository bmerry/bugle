/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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

#ifndef BUGLE_GL_GLDUMP_H
#define BUGLE_GL_GLDUMP_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <stdbool.h>
#include <stdio.h>
#include <budgie/types.h>

/* Takes a type specified by a GL token e.g. GL_FLOAT and returns the budgie_type.
 * The _ptr variant returns the budgie_type variant for a pointer to
 * that type. The _pbo_source and _pbo_sink variant first check if a PBO
 * source/sink is active, and if so returns an integral type of suitable size.
 */
budgie_type bugle_gl_type_to_type(GLenum gl_type);
budgie_type bugle_gl_type_to_type_ptr(GLenum gl_type);
budgie_type bugle_gl_type_to_type_ptr_pbo_source(GLenum gl_type);
budgie_type bugle_gl_type_to_type_ptr_pbo_sink(GLenum gl_type);
size_t bugle_gl_type_to_size(GLenum gl_type);
int bugle_gl_format_to_count(GLenum format, GLenum type);

/* Initialiser for certain dumping functions */
void dump_initialise(void);

int bugle_count_gl(budgie_function func, GLenum token);
#ifdef GL_ARB_vertex_program
int bugle_count_program_string(GLenum target, GLenum pname);
#endif
#ifdef GL_ARB_shader_objects
int bugle_count_attached_objects(GLhandleARB program, GLsizei max);
#endif

bool bugle_dump_convert(GLenum pname, const void *value,
                        budgie_type in_type, char **buffer, size_t *size);

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

#endif /* !BUGLE_GL_GLDUMP_H */
