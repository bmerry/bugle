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

/* Wrappers that call functions from whichever flavour of FBO is present.
 * These are provided because ES1 has only OES versions, ES2 has only core
 * versions, and GL has EXT and core versions which have different semantics.
 */

#ifndef BUGLE_GL_GLFBO_H
#define BUGLE_GL_GLFBO_H

#include <bugle/gl/glheaders.h>
#include <bugle/porting.h>
#include <bugle/export.h>
#include <bugle/filters.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Detects whether an FBO-capable core version or extension is
 * present in the current context.
 */
BUGLE_EXPORT_PRE bugle_bool bugle_gl_fbo_support(void) BUGLE_EXPORT_POST;

/* Retrieves the draw framebuffer (if there are separate read and draw targets),
 * or the framebuffer if there is only one target.
 */
BUGLE_EXPORT_PRE GLuint bugle_gl_get_draw_fbo(void) BUGLE_EXPORT_POST;

/* Call the GL function with the same name, picking the variant with the right
 * suffix.  Note that ARB_framebuffer_object and EXT_framebuffer_object have
 * different semantics for binding (ARB requireds a generated name). If both
 * are available, the ARB version will be used).
 */
BUGLE_EXPORT_PRE GLboolean BUDGIEAPI bugle_glIsRenderbuffer(GLuint renderbuffer) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glBindRenderbuffer(GLenum target, GLuint renderbuffer) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE GLboolean BUDGIEAPI bugle_glIsFramebuffer(GLuint framebuffer) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glBindFramebuffer(GLenum target, GLuint framebuffer) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glGenFramebuffers(GLsizei n, GLuint *framebuffers) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE GLenum BUDGIEAPI bugle_glCheckFramebufferStatus(GLenum target) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLint texture, GLint level) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void BUDGIEAPI bugle_glGenerateMipmap(GLenum target) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glIsRenderbuffer(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glBindRenderbuffer(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glDeleteRenderbuffer(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glGenRenderbuffers(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glRenderbufferStorage(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glGetRenderbufferParameteriv(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glIsFramebuffer(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glBindFramebuffer(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glDeleteFramebuffers(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glGenFramebuffers(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glCheckFramebufferStatus(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glFramebufferTexture2D(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glFramebufferRenderbuffer(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glGetFramebufferAttachmentParameteriv(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_gl_filter_catches_glGenerateMipmap(filter *f, bugle_bool inactive, filter_callback callback) BUGLE_EXPORT_POST;


/* TODO: make sure that the enums are defined with undecorated names,
 * even in ES1.1.
 */

#ifdef __cplusplus
}
#endif

#endif /* BUGLE_GL_GLFBO_H */
