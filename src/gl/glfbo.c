/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2011  Bruce Merry
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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define GL_GLEXT_PROTOTYPES
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glextensions.h>
#include <bugle/gl/glfbo.h>
#include <bugle/gl/glutils.h>
#include <bugle/filters.h>
#include <budgie/call.h>

#if BUGLE_GLTYPE_GLES2
# define call(x) (CALL(x))
#elif BUGLE_GLTYPE_GLES1
# define call(x) (CALL(x ## OES))
#elif BUGLE_GLTYPE_GL
# define call(x) (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_framebuffer_object) ? (CALL(x)) : (CALL(x ## EXT)))
#endif

bugle_bool bugle_gl_has_framebuffer_object(void)
{
#if BUGLE_GLTYPE_GLES2
    return true;
#elif BUGLE_GLTYPE_GLES1
    return BUGLE_GL_HAS_EXTENSION_GROUP(GL_OES_framebuffer_object);
#elif BUGLE_GLTYPE_GL
    return BUGLE_GL_HAS_EXTENSION_GROUP(GL_EXT_framebuffer_object);
#else
    return false;
#endif
}

GLuint bugle_gl_get_draw_framebuffer_binding(void)
{
    GLint fbo;
#if BUGLE_GLTYPE_GL
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_EXT_framebuffer_blit))
    {
        CALL(glGetIntegerv)(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
        return fbo;
    }
#endif
#if BUGLE_GLTYPE_GL || BUGLE_GLTYPE_GLES2
    CALL(glGetIntegerv)(GL_FRAMEBUFFER_BINDING, &fbo);
    return fbo;
#elif BUGLE_GLTYPE_GLES1
    CALL(glGetIntegerv)(GL_FRAMEBUFFER_BINDING_OES, &fbo);
    return fbo;
#else
    return 0;
#endif
}

GLenum bugle_gl_draw_framebuffer_target(void)
{
    GLenum target;
#if BUGLE_GLTYPE_GL
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_EXT_framebuffer_blit))
        target = GL_DRAW_FRAMEBUFFER;
    else
        target = GL_FRAMEBUFFER;
#elif BUGLE_GLTYPE_GLES1
    target = GL_FRAMEBUFFER_OES;
#else
    target = GL_FRAMEBUFFER;
#endif
    return target;
}

void bugle_gl_bind_draw_framebuffer(GLuint framebuffer)
{
    bugle_glBindFramebuffer(bugle_gl_draw_framebuffer_target(), framebuffer);
}

GLuint bugle_gl_get_read_framebuffer_binding(void)
{
    GLint fbo;
#if BUGLE_GLTYPE_GL
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_EXT_framebuffer_blit))
    {
        CALL(glGetIntegerv)(GL_READ_FRAMEBUFFER_BINDING, &fbo);
        return fbo;
    }
#endif
#if BUGLE_GLTYPE_GL || BUGLE_GLTYPE_GLES2
    CALL(glGetIntegerv)(GL_FRAMEBUFFER_BINDING, &fbo);
    return fbo;
#elif BUGLE_GLTYPE_GLES1
    CALL(glGetIntegerv)(GL_FRAMEBUFFER_BINDING_OES, &fbo);
    return fbo;
#else
    return 0;
#endif
}

GLenum bugle_gl_read_framebuffer_target(void)
{
    GLenum target;
#if BUGLE_GLTYPE_GL
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_EXT_framebuffer_blit))
        target = GL_READ_FRAMEBUFFER;
    else
        target = GL_FRAMEBUFFER;
#elif BUGLE_GLTYPE_GLES1
    target = GL_FRAMEBUFFER_OES;
#else
    target = GL_FRAMEBUFFER;
#endif
    return target;
}

void bugle_gl_bind_read_framebuffer(GLuint framebuffer)
{
    bugle_glBindFramebuffer(bugle_gl_read_framebuffer_target(), framebuffer);
}

GLboolean BUDGIEAPI bugle_glIsRenderbuffer(GLuint renderbuffer)
{
    return call(glIsRenderbuffer)(renderbuffer);
}

void BUDGIEAPI bugle_glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    call(glBindRenderbuffer)(target, renderbuffer);
}

void BUDGIEAPI bugle_glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
    call(glDeleteRenderbuffers)(n, renderbuffers);
}

void BUDGIEAPI bugle_glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
    call(glGenRenderbuffers)(n, renderbuffers);
}

void BUDGIEAPI bugle_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    call(glRenderbufferStorage)(target, internalformat, width, height);
}

void BUDGIEAPI bugle_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    call(glGetRenderbufferParameteriv)(target, pname, params);
}

GLboolean BUDGIEAPI bugle_glIsFramebuffer(GLuint framebuffer)
{
    return call(glIsFramebuffer)(framebuffer);
}

void BUDGIEAPI bugle_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    call(glBindFramebuffer)(target, framebuffer);
}

void BUDGIEAPI bugle_glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
    call(glDeleteFramebuffers)(n, framebuffers);
}

void BUDGIEAPI bugle_glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
    call(glGenFramebuffers)(n, framebuffers);
}

GLenum BUDGIEAPI bugle_glCheckFramebufferStatus(GLenum target)
{
    return call(glCheckFramebufferStatus)(target);
}

void BUDGIEAPI bugle_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLint texture, GLint level)
{
    call(glFramebufferTexture2D)(target, attachment, textarget, texture, level);
}

void BUDGIEAPI bugle_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    call(glFramebufferRenderbuffer)(target, attachment, renderbuffertarget, renderbuffer);
}

void BUDGIEAPI bugle_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
    call(glGetFramebufferAttachmentParameteriv)(target, attachment, pname, params);
}

void BUDGIEAPI bugle_glGenerateMipmap(GLenum target)
{
    call(glGenerateMipmap)(target);
}

#ifndef STRINGIFY
# define STRINGIFY2(x) #x
# define STRINGIFY(x) STRINGIFY2(x)
#endif

#define CATCHER_WRAPPER(func) \
    void bugle_gl_filter_catches_ ## func (filter *f, bugle_bool inactive, filter_callback callback) \
    { \
        CATCH_WRAPPER_INNER(func); \
    }

#if BUGLE_GLTYPE_GLES2
# define CATCHER_WRAPPER_INNER(func) \
    bugle_filter_catches_function(f, STRINGIFY(func), inactive, callback)
#elif BUGLE_GLTYPE_GLES1
# define CATCHER_WRAPPER_INNER(func) \
    bugle_filter_catches_function(f, STRINGIFY(func ## OES), inactive, callback)
#elif BUGLE_GLTYPE_GL
# define CATCH_WRAPPER_INNER(func) \
    bugle_filter_catches_function(f, STRINGIFY(func), inactive, callback); \
    bugle_filter_catches_function(f, STRINGIFY(func ## EXT), inactive, callback)
#endif

CATCHER_WRAPPER(glIsRenderbuffer)
CATCHER_WRAPPER(glBindRenderbuffer)
CATCHER_WRAPPER(glDeleteRenderbuffers)
CATCHER_WRAPPER(glGenRenderbuffers)
CATCHER_WRAPPER(glRenderbufferStorage)
CATCHER_WRAPPER(glGetRenderbufferParameteriv)
CATCHER_WRAPPER(glIsFramebuffer)
CATCHER_WRAPPER(glBindFramebuffer)
CATCHER_WRAPPER(glDeleteFramebuffers)
CATCHER_WRAPPER(glGenFramebuffers)
CATCHER_WRAPPER(glCheckFramebufferStatus)
CATCHER_WRAPPER(glFramebufferTexture2D)
CATCHER_WRAPPER(glFramebufferRenderbuffer)
CATCHER_WRAPPER(glGetFramebufferAttachmentParameteriv)
CATCHER_WRAPPER(glGenerateMipmap)
