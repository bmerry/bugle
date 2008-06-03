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

#ifndef BUGLE_GLWIN_H
#define BUGLE_GLWIN_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/filters.h>
#include <bugle/porting.h>
#include <bugle/glwintypes.h>
#include <budgie/types2.h>
#include <stdbool.h>

/* Wrappers around GLX/WGL/EGL functions */
glwin_display  bugle_glwin_get_current_display(void);
glwin_context  bugle_glwin_get_current_context(void);
glwin_drawable bugle_glwin_get_current_drawable(void);
glwin_drawable bugle_glwin_get_current_read_drawable(void);

bool bugle_glwin_make_context_current(glwin_display dpy, glwin_drawable draw,
                                      glwin_drawable read, glwin_context ctx);


/* Wrapper around glXGetProcAddress or similar functios */
void (BUDGIEAPI *bugle_glwin_get_proc_address(const char *name))(void);

/* Wrapper around glXQueryVersion - return 1.0 for wgl */
void bugle_glwin_query_version(glwin_display dpy, int *major, int *minor);

/* Returns the window-system extension string */
const char *bugle_glwin_query_extensions_string(glwin_display dpy);

/* Returns the width and height of the drawable, if it can be determined.
 * Otherwise returns -1, -1. Both parameters must be non-NULL.
 */
void bugle_glwin_get_drawable_dimensions(glwin_display dpy, glwin_drawable drawable,
                                         int *width, int *height);

/* Encodes information about the call that created a context, so that a similar
 * one may be created later. This is only the platform-independent part.
 * Each platform will append some data to the structure.
 */
typedef struct glwin_context_create
{
    budgie_function function;
    budgie_group group;
    glwin_display dpy;
    glwin_context share;
    glwin_context ctx;
} glwin_context_create;

/* Captures the necessary information from a context creation call to make
 * more contexts. Returns a newly allocated glwin_context_create (or a
 * subclass) if the original function succeeded, or NULL if it failed.
 * This should only be called after the original function completed.
 */
glwin_context_create *bugle_glwin_context_create_save(function_call *call);

/* Creates a new context that is compatible with the information captured in
 * the create structure. If share is true, the new context will share textures
 * etc with the original. Returns the new context, or NULL on failure.
 */
glwin_context bugle_glwin_context_create_new(const struct glwin_context_create *create, bool share);

/* Extracts the context passed to a destroy function */
glwin_context bugle_glwin_get_context_destroy(function_call *call);

/* Helper functions to trap the appropriate window-system calls */
void bugle_glwin_filter_catches_create_context(filter *f, bool inactive, filter_callback callback);
void bugle_glwin_filter_catches_destroy_context(filter *f, bool inactive, filter_callback callback);
void bugle_glwin_filter_catches_make_current(filter *f, bool inactive, filter_callback callback);
void bugle_glwin_filter_catches_swap_buffers(filter *f, bool inactive, filter_callback callback);

#endif /* !BUGLE_GLWIN_H */
