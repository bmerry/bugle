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

/* Wrapper that calls either GL 2.0 or ARB_shader_objects versions of
 * GLSL calls, depending on what is available. The functions are
 * named based on GL 2.0 conventions.
 */

#ifndef BUGLE_SRC_GLSL_H
#define BUGLE_SRC_GLSL_H

#include <GL/gl.h>
#include <GL/glext.h>

#ifdef GL_ARB_shader_objects
void glsl_glGetProgramiv(GLuint program, GLenum pname, GLint *param);
void glsl_glGetShaderiv(GLuint shader, GLenum pname, GLint *param);
void glsl_glGetAttachedShaders(GLuint program, GLsizei max_length, GLsizei *length, GLuint *shaders);
void glsl_glGetProgramInfoLog(GLuint program, GLsizei max_length, GLsizei *length, GLcharARB *log);
void glsl_glGetShaderInfoLog(GLuint shader, GLsizei max_length, GLsizei *length, GLcharARB *log);
void glsl_glGetShaderSource(GLuint shader, GLsizei max_length, GLsizei *length, GLcharARB *source);
void glsl_glGetActiveUniform(GLuint program, GLuint index, GLsizei max_length, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
void glsl_glGetActiveAttrib(GLuint program, GLuint index, GLsizei max_length, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
void glsl_glGetUniformfv(GLuint program, GLint location, GLfloat *params);
void glsl_glGetUniformiv(GLuint program, GLint location, GLint *params);
GLint glsl_glGetUniformLocation(GLuint program, const GLcharARB *name);
GLint glsl_glGetAttribLocation(GLuint program, const GLcharARB *name);
#endif /* GL_ARB_shader_objects */

#endif /* !BUGLE_SRC_GLSL_H */
