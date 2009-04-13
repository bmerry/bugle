/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008-2009  Bruce Merry
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

#ifndef BUGLE_GLWIN_GLWINTYPES_H
#define BUGLE_GLWIN_GLWINTYPES_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/porting.h>
#include <stdbool.h>
#include <stdio.h>

#if BUGLE_GLWIN_GLX
#include <GL/glx.h>
#include <GL/glxext.h>

typedef Display *   glwin_display;
typedef GLXContext  glwin_context;
typedef GLXDrawable glwin_drawable;
typedef Bool        glwin_bool;
typedef int         glwin_enum;
typedef int         glwin_attrib;     /* type for (attrib, value) pair list */

#define GLWIN_BOOL_TRUE "True"
#define GLWIN_BOOL_FALSE "False"
#define GLWIN_BOOL_TYPE "Bool"

#ifdef GLX_ARB_get_proc_address
# define BUGLE_GLWIN_GET_PROC_ADDRESS glXGetProcAddressARB
#endif
#endif /* BUGLE_GLWIN_GLX */

#if BUGLE_GLWIN_WGL
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* In Windows, display and surface are the same thing */
typedef HDC   glwin_display;
typedef HGLRC glwin_context;
typedef HDC   glwin_drawable;
typedef BOOL  glwin_bool;
typedef int   glwin_enum;
typedef int   glwin_attrib;

/* Don't put brackets around these: they are stringized for dumping */
#define GLWIN_BOOL_TRUE "TRUE"
#define GLWIN_BOOL_FALSE "FALSE"
#define GLWIN_BOOL_TYPE "BOOL"

#define BUGLE_GLWIN_GET_PROC_ADDRESS wglGetProcAddress
#endif /* BUGLE_GLWIN_WGL */

#if BUGLE_GLWIN_EGL
#include <EGL/egl.h>

typedef EGLDisplay glwin_display;
typedef EGLContext glwin_context;
typedef EGLSurface glwin_drawable;
typedef EGLBoolean glwin_bool;
typedef EGLenum    glwin_enum;
typedef EGLint     glwin_attrib;

#define GLWIN_BOOL_TRUE "EGL_TRUE"
#define GLWIN_BOOL_FALSE "EGL_FALSE"
#define GLWIN_BOOL_TYPE "EGLBoolean"

#define BUGLE_GLWIN_GET_PROC_ADDRESS eglGetProcAddress
#endif /* BUGLE_GLWIN_EGL */

bool bugle_dump_glwin_bool(glwin_bool b, char **buffer, size_t *size);
bool bugle_dump_glwin_drawable(glwin_drawable d, char **buffer, size_t *size);
bool bugle_dump_glwin_enum(glwin_enum e, char **buffer, size_t *size);

int bugle_count_glwin_attributes(const glwin_attrib *attribs, glwin_attrib terminator);
bool bugle_dump_glwin_attributes(const glwin_attrib *attribs, glwin_attrib terminator, char **buffer, size_t *size);

#endif /* BUGLE_GLWIN_GLWINTYPES_H */
