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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/wgl.h>
#include <bugle/glwintypes.h>
#include <bugle/glwin.h>
#include "xalloc.h"
#include "budgielib/defines.h"

glwin_display bugle_glwin_get_current_display(void)
{
    return wglGetCurrentDC();
}

glwin_context bugle_glwin_get_current_context(void)
{
    return wglGetCurrentContext();
}

glwin_drawable bugle_glwin_get_current_drawable(void)
{
    return wglGetCurrentDC();
}

glwin_drawable bugle_glwin_get_current_read_drawable(void)
{
    return wglGetCurrentReadDCARB();
}

bool bugle_glwin_make_context_current(glwin_display dpy, glwin_drawable draw,
                                      glwin_drawable read, glwin_context ctx)
{
    wglMakeContextCurrentARB(draw, read, ctx);
}

void (*bugle_glwin_get_proc_address(const char *name))(void)
{
    return wglGetProcAddress(name);
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
    return NULL;
}

void bugle_glwin_get_drawable_dimensions(glwin_display dpy, glwin_drawable drawable,
                                         int *width, int *height)
{
    /* FIXME: implement */
    *width = -1;
    *height = -1;
    return NULL;
}

typedef struct glwin_context_create_wgl
{
    glwin_context_create parent;
};

glwin_context_create *bugle_glwin_context_create_save(function_call *call)
{
    glwin_context ctx;
    glwin_context_create_wgl *create;

    ctx = *call->wglCreateContext.retn;
    if (!ctx)
        return NULL;
    create = XMALLOC(glwin_context_create_wgl);
    create->parent.dpy = *call->wglCreateContext.arg0;
    create->parent.function = call->function;
    create->parent.group = call->group;
    create->parent.ctx = ctx;
    create->parent.share = false;

    return (glwin_context_create *) create;
}

glwin_context bugle_glwin_context_create_new(const glwin_context_create *create, bool share)
{
    glwin_context ctx;
    ctx = CALL(wglContextCreate)(create->dpy);
    if (share)
        wglShareLists(create->ctx, ctx);
    return ctx;
}

void bugle_glwin_filter_catches_create_context(filter *f, bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "wglCreateContext", inactive, callback);
}

void bugle_glwin_filter_catches_make_current(filter *f, bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "wglMakeCurrent", inactive, callback);
    bugle_filter_catches(f, "wglMakeContextCurrentARB", inactive, callback);
}

void bugle_glwin_filter_catches_swap_buffers(filter *f, bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "wglSwapBuffers", inactive, callback);
}
