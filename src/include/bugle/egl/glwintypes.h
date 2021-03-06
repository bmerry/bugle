/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008  Bruce Merry
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

#ifndef BUGLE_EGL_GLWINTYPES_H
#define BUGLE_EGL_GLWINTYPES_H


#include <EGL/egl.h>
#include <EGL/eglext.h>

typedef EGLDisplay bugle_glwin_display;
typedef EGLContext bugle_glwin_context;
typedef EGLSurface bugle_glwin_drawable;
typedef EGLConfig  bugle_glwin_visual_info;

#endif /* !BUGLE_EGL_GLWINTYPES_H */
