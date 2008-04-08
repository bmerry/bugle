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

#ifndef BUGLE_GLX_GLWINTYPES_H
#define BUGLE_GLX_GLWINTYPES_H

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <GL/glx.h>
#include <GL/glxext.h>

typedef Display *   bugle_glwin_display;
typedef GLXContext  bugle_glwin_context;
typedef GLXDrawable bugle_glwin_drawable;
typedef XVisualInfo bugle_glwin_visual_info;

#endif /* !BUGLE_GLX_GLWINTYPES_H */
