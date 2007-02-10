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

/* Wrapper that calls either GL 2.0 or ARB_shader_objects versions of
 * GLSL calls, depending on what is available. The functions are
 * named based on GL 2.0 conventions.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include "src/tracker.h"
#include "src/glutils.h"
#include "src/utils.h"
#include "src/glexts.h"
#include "common/safemem.h"

#ifdef GL_VERSION_2_0
#define call1(gl,arb,params) \
    do { \
        if (bugle_gl_has_extension(BUGLE_GL_VERSION_2_0)) \
        { \
            CALL_ ## gl params; \
        } \
        else \
        { \
            CALL_ ## arb params; \
        } \
    } while (0)
#else
#define call1(gl,arb,params) arb params
#endif

#ifdef GL_ARB_shader_objects
void bugle_glGetProgramiv(GLuint program, GLenum pname, GLint *param)
{
    call1(glGetProgramiv, glGetObjectParameterivARB, (program, pname, param));
}

void bugle_glGetShaderiv(GLuint shader, GLenum pname, GLint *param)
{
    call1(glGetShaderiv, glGetObjectParameterivARB, (shader, pname, param));
}

void bugle_glGetAttachedShaders(GLuint program, GLsizei max_length, GLsizei *length, GLuint *shaders)
{
#ifdef GL_VERSION_2_0
    if (bugle_gl_has_extension(GL_VERSION_2_0))
    {
        CALL_glGetAttachedShaders(program, max_length, length, shaders);
    }
    else
#endif
    {
        GLhandleARB *handles;
        GLsizei i;
        GLsizei my_length;

        handles = bugle_malloc(max_length * sizeof(GLhandleARB));
        CALL_glGetAttachedObjectsARB(program, max_length, &my_length, handles);
        for (i = 0; i < my_length; i++)
            shaders[i] = handles[i];
        if (length) *length = my_length;
        free(handles);
    }
}

void bugle_glGetProgramInfoLog(GLuint program, GLsizei max_length, GLsizei *length, GLcharARB *log)
{
    call1(glGetProgramInfoLog, glGetInfoLogARB, (program, max_length, length, log));
}

void bugle_glGetShaderInfoLog(GLuint shader, GLsizei max_length, GLsizei *length, GLcharARB *log)
{
    call1(glGetShaderInfoLog, glGetInfoLogARB, (shader, max_length, length, log));
}

void bugle_glGetShaderSource(GLuint shader, GLsizei max_length, GLsizei *length, GLcharARB *source)
{
    call1(glGetShaderSource, glGetShaderSourceARB, (shader, max_length, length, source));
}

void bugle_glGetActiveUniform(GLuint program, GLuint index, GLsizei max_length, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name)
{
    call1(glGetActiveUniform, glGetActiveUniformARB, (program, index, max_length, length, size, type, name));
}

void bugle_glGetActiveAttrib(GLuint program, GLuint index, GLsizei max_length, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name)
{
    call1(glGetActiveAttrib, glGetActiveAttribARB, (program, index, max_length, length, size, type, name));
}

void bugle_glGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
    call1(glGetUniformfv, glGetUniformfvARB, (program, location, params));
}

void bugle_glGetUniformiv(GLuint program, GLint location, GLint *params)
{
    call1(glGetUniformiv, glGetUniformivARB, (program, location, params));
}

GLint bugle_glGetUniformLocation(GLuint program, const GLcharARB *name)
{
#ifdef GL_VERSION_2_0
    if (bugle_gl_has_extension(GL_VERSION_2_0))
        return CALL_glGetUniformLocation(program, name);
#endif
    return CALL_glGetUniformLocationARB(program, name);
}

GLint bugle_glGetAttribLocation(GLuint program, const GLcharARB *name)
{
#ifdef GL_VERSION_2_0
    if (bugle_gl_has_extension(GL_VERSION_2_0))
        return CALL_glGetAttribLocation(program, name);
#endif
    return CALL_glGetAttribLocationARB(program, name);
}

#endif /* GL_ARB_shader_objects */
