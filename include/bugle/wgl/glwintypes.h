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

#ifndef BUGLE_WGL_GLWINTYPES_H
#define BUGLE_WGL_GLWINTYPES_H

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/wgl.h>
#include <GL/wglext.h>

/* In Windows, display and surface are the same thing */
typedef HDC   bugle_glwin_display;
typedef HGLRC bugle_glwin_context;
typedef HDC   bugle_glwin_drawable;

#define BUGLE_GLWIN_GET_PROC_ADDRESS wglGetProcAddress

#endif /* !BUGLE_WGL_GLWINTYPES_H */

