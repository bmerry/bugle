/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2010  Bruce Merry
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

#include <bugle/gl/glheaders.h>
#include <bugle/bool.h>
#include <stdio.h>
#include <budgie/types.h>
#include <bugle/export.h>
#include <bugle/io.h>

/* Takes a type specified by a GL token e.g. GL_FLOAT and returns the budgie_type.
 * The _ptr variant returns the budgie_type variant for a pointer to
 * that type. The _pbo_source and _pbo_sink variant first check if a PBO
 * source/sink is active, and if so returns an integral type of suitable size.
 * Similarly, the _vbo_element version first checks if an element array buffer
 * is active.
 */
BUGLE_EXPORT_PRE budgie_type bugle_gl_type_to_type(GLenum gl_type) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE budgie_type bugle_gl_type_to_type_ptr(GLenum gl_type) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE budgie_type bugle_gl_type_to_type_ptr_pbo_source(GLenum gl_type) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE budgie_type bugle_gl_type_to_type_ptr_pbo_sink(GLenum gl_type) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE budgie_type bugle_gl_type_to_type_ptr_vbo_element(GLenum gl_type) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE size_t bugle_gl_type_to_size(GLenum gl_type) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int bugle_gl_format_to_count(GLenum format, GLenum type) BUGLE_EXPORT_POST;

/* Initialiser for certain dumping functions */
void dump_initialise(void);

BUGLE_EXPORT_PRE int bugle_count_gl(budgie_function func, GLenum token) BUGLE_EXPORT_POST;
#ifdef GL_ARB_vertex_program
BUGLE_EXPORT_PRE int bugle_count_program_string(GLenum target, GLenum pname) BUGLE_EXPORT_POST;
#endif
BUGLE_EXPORT_PRE int bugle_count_attached_shaders(GLuint program, GLsizei max) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE bugle_bool bugle_dump_convert(GLenum pname, const void *value,
                                               budgie_type in_type, bugle_io_writer *writer) BUGLE_EXPORT_POST;

/* Dumps the indices array of glMultiDrawElements, showing the appropriate
 * number of elements for each element of indices.
 */
BUGLE_EXPORT_PRE bugle_bool bugle_gl_dump_multi_draw_elements(const GLsizei *count, GLenum gl_type, const GLvoid * const *indices, GLsizei primcount, bugle_io_writer *writer) BUGLE_EXPORT_POST;

/* Computes the number of pixel elements (units of byte, int, float etc)
 * used by a client-side encoding of a 1D, 2D or 3D image.
 * Specify -1 for depth for 1D or 2D textures.
 */
BUGLE_EXPORT_PRE size_t bugle_image_element_count(GLsizei width,
                                                  GLsizei height,
                                                  GLsizei depth,
                                                  GLenum format,
                                                  GLenum type,
                                                  bugle_bool unpack) BUGLE_EXPORT_POST;

/* Computes the number of pixel elements required by glGetTexImage
 */
BUGLE_EXPORT_PRE size_t bugle_texture_element_count(GLenum target,
                                                    GLint level,
                                                    GLenum format,
                                                    GLenum type) BUGLE_EXPORT_POST;

#endif /* !BUGLE_GL_GLDUMP_H */
