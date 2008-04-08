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

#ifndef BUGLE_GLX_GLWIN_H
#define BUGLE_GLX_GLWIN_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/glwintypes.h>
#include <stdbool.h>

/* Wrappers around GLX/WGL/EGL functions */
bugle_glwin_display  bugle_get_current_display(void);
bugle_glwin_context  bugle_get_current_context(void);
bugle_glwin_drawable bugle_get_current_drawable(void);
bugle_glwin_drawable bugle_get_current_read_drawable(void);

bool bugle_make_context_current(bugle_glwin_display dpy, bugle_glwin_drawable draw,
                                bugle_glwin_drawable read, bugle_glwin_context ctx);

/* Creates a new context with the same visual/pixel format/config as the
 * given one. If 'share' is true, it will share texture data etc, otherwise
 * it will not. Returns NULL on failure.
 *
 * This functionality is intended primarily for trackcontext to generate
 * aux contexts. Most user code should use those aux contexts rather than
 * generating lots of their own.
 */
bugle_glwin_context bugle_clone_current_context(bugle_glwin_display dpy,
                                                bugle_glwin_context ctx,
                                                bool share);
#endif /* !BUGLE_GLX_GLWIN_H */
