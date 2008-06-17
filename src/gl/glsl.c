/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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
#define GL_GLEXT_PROTOTYPES
#include <stdlib.h>
#include <string.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glextensions.h>
#include <bugle/gl/glutils.h>
#include <budgie/call.h>
#include "src/apitables.h"
#include "xalloc.h"

#if BUGLE_GLTYPE_GLES2
#define call1(gl,arb,params) CALL(gl) params
#define call1r(gl,arb,params) return CALL(gl) params
#elif defined(GL_VERSION_2_0)
#define call1(gl,arb,params) \
    do { \
        if (BUGLE_GL_HAS_EXTENSION(GL_VERSION_2_0)) \
        { \
            CALL(gl) params; \
        } \
        else \
        { \
            CALL(arb) params; \
        } \
    } while (0)
#define call1r(gl,arb,params) \
    do { \
        if (BUGLE_GL_HAS_EXTENSION(GL_VERSION_2_0)) \
        { \
            return CALL(gl) params; \
        } \
        else \
        { \
            return CALL(arb) params; \
        } \
    } while (0)
#else
#define call1(gl,arb,params) CALL(arb) params
#define call1r(gl,arb,params) return CALL(arb) params
#endif

#if defined(GL_ARB_shader_objects) || defined(GL_ES_VERSION_2_0)
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
    call1r(glGetAttachedShaders, glGetAttachedObjectsARB, (program, max_length, length, shaders));
}

void bugle_glGetProgramInfoLog(GLuint program, GLsizei max_length, GLsizei *length, char *log)
{
    call1(glGetProgramInfoLog, glGetInfoLogARB, (program, max_length, length, log));
}

void bugle_glGetShaderInfoLog(GLuint shader, GLsizei max_length, GLsizei *length, char *log)
{
    call1(glGetShaderInfoLog, glGetInfoLogARB, (shader, max_length, length, log));
}

void bugle_glGetShaderSource(GLuint shader, GLsizei max_length, GLsizei *length, char *source)
{
    call1(glGetShaderSource, glGetShaderSourceARB, (shader, max_length, length, source));
}

void bugle_glGetActiveUniform(GLuint program, GLuint index, GLsizei max_length, GLsizei *length, GLint *size, GLenum *type, char *name)
{
    call1(glGetActiveUniform, glGetActiveUniformARB, (program, index, max_length, length, size, type, name));
}

void bugle_glGetActiveAttrib(GLuint program, GLuint index, GLsizei max_length, GLsizei *length, GLint *size, GLenum *type, char *name)
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

GLint bugle_glGetUniformLocation(GLuint program, const char *name)
{
    call1r(glGetUniformLocation, glGetUniformLocationARB, (program, name));
}

GLint bugle_glGetAttribLocation(GLuint program, const char *name)
{
    call1r(glGetAttribLocation, glGetAttribLocationARB, (program, name));
}

#endif /* GL_ARB_shader_objects || GL_ES_VERSION_2_0 */

#ifdef GL_ARB_shader_objects
GLuint bugle_gl_get_current_program(void)
{
    GLint handle;
#ifdef GL_VERSION_2_0
    if (BUGLE_GL_HAS_EXTENSION(GL_VERSION_2_0))
    {
        CALL(glGetIntegerv)(GL_CURRENT_PROGRAM, &handle);
        return handle;
    }
#endif
    return CALL(glGetHandleARB)(GL_PROGRAM_OBJECT_ARB);
}

GLboolean bugle_glIsShader(GLuint shader)
{
    GLint type;
#ifdef GL_VERSION_2_0
    if (BUGLE_GL_HAS_EXTENSION(GL_VERSION_2_0))
        return CALL(glIsShader)(shader);
#endif
    CALL(glGetObjectParameterivARB)(shader, GL_OBJECT_TYPE_ARB, &type);
    return (CALL(glGetError)() == GL_NO_ERROR
            && type == GL_SHADER_OBJECT_ARB);
}

GLboolean bugle_glIsProgram(GLuint program)
{
    GLint type;
#ifdef GL_VERSION_2_0
    if (BUGLE_GL_HAS_EXTENSION(GL_VERSION_2_0))
        return CALL(glIsProgram)(program);
#endif
    CALL(glGetObjectParameterivARB)(program, GL_OBJECT_TYPE_ARB, &type);
    return (CALL(glGetError)() == GL_NO_ERROR
            && type == GL_PROGRAM_OBJECT_ARB);
}
#endif /* GL_ARB_shader_objects */

#ifdef GL_ES_VERSION_2_0
GLuint bugle_gl_get_current_program(void)
{
    GLint handle;
    CALL(glGetIntegerv)(GL_CURRENT_PROGRAM, &handle);
    return handle;
}

GLboolean bugle_glIsShader(GLuint shader)
{
    return CALL(glIsShader)(shader);
}

GLboolean bugle_glIsProgram(GLuint program)
{
    return CALL(glIsProgram)(program);
}
#endif
