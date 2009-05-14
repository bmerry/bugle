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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define WGL_WGLEXT_PROTOTYPES
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bugle/bool.h>
#include <bugle/glwin/glwin.h>
#include <GL/wglext.h>
#include <budgie/call.h>
#include "xalloc.h"
#include "budgielib/defines.h"

glwin_display bugle_glwin_get_current_display(void)
{
    return CALL(wglGetCurrentDC)();
}

glwin_context bugle_glwin_get_current_context(void)
{
    return CALL(wglGetCurrentContext)();
}

glwin_drawable bugle_glwin_get_current_drawable(void)
{
    return CALL(wglGetCurrentDC)();
}

glwin_drawable bugle_glwin_get_current_read_drawable(void)
{
    /* FIXME: use wglGetCurrentReadDCARB if available - but it needs a
     * context to obtain the address.
     */
    return CALL(wglGetCurrentDC)();
}

bugle_bool bugle_glwin_make_context_current(glwin_display dpy, glwin_drawable draw,
                                            glwin_drawable read, glwin_context ctx)
{
    /* FIXME: use wglMakeContextCurrentARB if available - but it needs a
     * context to obtain the address.
     */
    return CALL(wglMakeCurrent)(draw, ctx);
}

BUDGIEAPIPROC bugle_glwin_get_proc_address(const char *name)
{
    return (BUDGIEAPIPROC) CALL(wglGetProcAddress)(name);
}

void bugle_glwin_query_version(glwin_display dpy, int *major, int *minor)
{
    *major = 1;
    *minor = 0;
}

const char *bugle_glwin_query_extensions_string(glwin_display dpy)
{
    /* FIXME: This gets ugly, because it is itself an extension function and is in
     * the ICD, so we have to have an appropriate context because we can even
     * get a handle to the function.
     */
    return "";
}

void bugle_glwin_get_drawable_dimensions(glwin_display dpy, glwin_drawable drawable,
                                         int *width, int *height)
{
    /* There is no WGL equivalent to glXQueryDrawable. However, we can get the
     * dimensions by attaching a brand-new context, whose viewport will
     * automatically take the size of the drawable surface.
     */
    glwin_context ctx, old_ctx;
    glwin_drawable old_draw, old_read;
    glwin_display old_dpy;

    old_dpy = bugle_glwin_get_current_display();
    old_draw = bugle_glwin_get_current_drawable();
    old_read = bugle_glwin_get_current_read_drawable();
    old_ctx = bugle_glwin_get_current_context();

    ctx = CALL(wglCreateContext)(drawable);
    if (ctx && CALL(wglMakeCurrent)(drawable, ctx))
    {
        GLint viewport[4];
        CALL(glGetIntegerv)(GL_VIEWPORT, viewport);
        *width = viewport[2];
        *height = viewport[3];

        bugle_glwin_make_context_current(old_dpy, old_draw, old_read, old_ctx);
        wglDeleteContext(ctx);
    }
    else
    {
        *width = -1;
        *height = -1;
    }
}

typedef struct
{
    glwin_context_create parent;
} glwin_context_create_wgl;

glwin_context_create *bugle_glwin_context_create_save(function_call *call)
{
    glwin_context ctx;
    glwin_context_create_wgl *create;

    ctx = *call->wglCreateContext.retn;
    if (!ctx)
        return NULL;
    create = XMALLOC(glwin_context_create_wgl);
    create->parent.dpy = *call->wglCreateContext.arg0;
    create->parent.function = call->generic.id;
    create->parent.group = call->generic.group;
    create->parent.ctx = ctx;
    create->parent.share = BUGLE_FALSE;

    return (glwin_context_create *) create;
}

glwin_context bugle_glwin_context_create_new(const glwin_context_create *create, bugle_bool share)
{
    glwin_context ctx;
    ctx = CALL(wglCreateContext)(create->dpy);
    if (share)
        CALL(wglShareLists)(create->ctx, ctx);
    return ctx;
}

glwin_context bugle_glwin_get_context_destroy(function_call *call)
{
    return *call->wglDeleteContext.arg0;
}

void bugle_glwin_filter_catches_create_context(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "wglCreateContext", inactive, callback);
}

void bugle_glwin_filter_catches_destroy_context(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "wglDeleteContext", inactive, callback);
}

void bugle_glwin_filter_catches_make_current(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "wglMakeCurrent", inactive, callback);
    bugle_filter_catches(f, "wglMakeContextCurrentARB", inactive, callback);
}

void bugle_glwin_filter_catches_swap_buffers(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "wglSwapBuffers", inactive, callback);
}

void bugle_function_address_initialise_extra(void)
{
    /* This doesn't work for WGL because it uses context-dependent function
     * pointers. Instead, budgie_address_generator returns addresses on the
     * fly.
     */
}
