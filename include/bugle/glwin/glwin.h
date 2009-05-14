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

#ifndef BUGLE_GLWIN_H
#define BUGLE_GLWIN_H

#include <bugle/filters.h>
#include <bugle/porting.h>
#include <bugle/glwin/glwintypes.h>
#include <budgie/types2.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wrappers around GLX/WGL/EGL functions */
BUGLE_EXPORT_PRE glwin_display  bugle_glwin_get_current_display(void) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE glwin_context  bugle_glwin_get_current_context(void) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE glwin_drawable bugle_glwin_get_current_drawable(void) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE glwin_drawable bugle_glwin_get_current_read_drawable(void) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE bool bugle_glwin_make_context_current(glwin_display dpy, glwin_drawable draw,
                                                       glwin_drawable read, glwin_context ctx) BUGLE_EXPORT_POST;


/* Wrapper around glXGetProcAddress or similar functios */
BUGLE_EXPORT_PRE BUDGIEAPIPROC bugle_glwin_get_proc_address(const char *name) BUGLE_EXPORT_POST;

/* Wrapper around glXQueryVersion - return 1.0 for wgl */
BUGLE_EXPORT_PRE void bugle_glwin_query_version(glwin_display dpy, int *major, int *minor) BUGLE_EXPORT_POST;

/* Returns the window-system extension string */
BUGLE_EXPORT_PRE const char *bugle_glwin_query_extensions_string(glwin_display dpy) BUGLE_EXPORT_POST;

/* Returns the width and height of the drawable, if it can be determined.
 * Otherwise returns -1, -1. Both parameters must be non-NULL.
 */
BUGLE_EXPORT_PRE void bugle_glwin_get_drawable_dimensions(glwin_display dpy, glwin_drawable drawable,
                                                          int *width, int *height) BUGLE_EXPORT_POST;

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
BUGLE_EXPORT_PRE glwin_context_create *bugle_glwin_context_create_save(function_call *call)BUGLE_EXPORT_POST;

/* Creates a new context that is compatible with the information captured in
 * the create structure. If share is true, the new context will share textures
 * etc with the original. Returns the new context, or NULL on failure.
 */
BUGLE_EXPORT_PRE glwin_context bugle_glwin_context_create_new(const struct glwin_context_create *create, bool share) BUGLE_EXPORT_POST;

/* Extracts the context passed to a destroy function */
BUGLE_EXPORT_PRE glwin_context bugle_glwin_get_context_destroy(function_call *call) BUGLE_EXPORT_POST;

/* Helper functions to trap the appropriate window-system calls */
BUGLE_EXPORT_PRE void bugle_glwin_filter_catches_create_context(filter *f, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_glwin_filter_catches_destroy_context(filter *f, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_glwin_filter_catches_make_current(filter *f, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_glwin_filter_catches_swap_buffers(filter *f, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;

/* Fills in function pointer table for extensions */
BUGLE_EXPORT_PRE void bugle_function_address_initialise_extra(void) BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_GLWIN_H */
