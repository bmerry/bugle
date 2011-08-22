/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008, 2011  Bruce Merry
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

#include <EGL/egl.h>
#include <budgie/call.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <string.h>
#include <bugle/glwin/glwin.h>
#include <bugle/apireflect.h>
#include "budgielib/defines.h"

glwin_display bugle_glwin_get_current_display(void)
{
    return CALL(eglGetCurrentDisplay)();
}

glwin_context bugle_glwin_get_current_context(void)
{
    return CALL(eglGetCurrentContext)();
}

glwin_drawable bugle_glwin_get_current_drawable(void)
{
    return CALL(eglGetCurrentSurface)(EGL_DRAW);
}

glwin_drawable bugle_glwin_get_current_read_drawable(void)
{
    return CALL(eglGetCurrentSurface)(EGL_READ);
}

bugle_bool bugle_glwin_make_context_current(glwin_display dpy, glwin_drawable draw,
                                            glwin_drawable read, glwin_context ctx)
{
    return CALL(eglMakeCurrent)(dpy, draw, read, ctx);
}

BUDGIEAPIPROC bugle_glwin_get_proc_address(const char *name)
{
    return (BUDGIEAPIPROC) CALL(eglGetProcAddress)(name);
}

void bugle_glwin_query_version(glwin_display dpy, int *major, int *minor)
{
    /* FIXME: is this valid? We are reinitialising! */
    CALL(eglInitialize)(dpy, major, minor);
}

const char *bugle_glwin_query_extensions_string(glwin_display dpy)
{
    return CALL(eglQueryString)(dpy, EGL_EXTENSIONS);
}

void bugle_glwin_get_drawable_dimensions(glwin_display dpy, glwin_drawable drawable,
                                         int *width, int *height)
{
    EGLint w, h;
    CALL(eglQuerySurface)(dpy, drawable, EGL_WIDTH, &w);
    CALL(eglQuerySurface)(dpy, drawable, EGL_HEIGHT, &h);
    *width = w;
    *height = h;
}

typedef struct
{
    glwin_context_create parent;
    EGLConfig config;
    EGLint *attribs;
} glwin_context_create_egl;

glwin_context_create *bugle_glwin_context_create_save(function_call *call)
{
    glwin_context ctx;
    glwin_context_create_egl *create;
    EGLint nattribs = 0;
    const EGLint *attribs;

    ctx = *call->eglCreateContext.retn;
    if (!ctx)
        return NULL;
    attribs = *call->eglCreateContext.arg3;
    if (attribs != NULL)
    {
        while (attribs[nattribs] != EGL_NONE)
            nattribs += 2;
        nattribs++;
    }

    create = BUGLE_MALLOC(glwin_context_create_egl);
    create->parent.dpy = *call->eglCreateContext.arg0;
    create->parent.function = call->generic.id;
    create->parent.group = call->generic.group;
    create->parent.ctx = ctx;
    create->parent.share = BUGLE_FALSE;
    create->config = *call->eglCreateContext.arg1;
    if (attribs != NULL)
    {
        create->attribs = BUGLE_NMALLOC(nattribs, EGLint);
        memcpy(create->attribs, attribs, nattribs * sizeof(EGLint));
    }
    else
        create->attribs = NULL;

    return (glwin_context_create *) create;
}

glwin_context bugle_glwin_context_create_new(const glwin_context_create *create, bugle_bool share)
{
    glwin_context_create_egl *c;

    c = (glwin_context_create_egl *) create;
    return CALL(eglCreateContext)(create->dpy, c->config, share ? create->ctx : NULL, c->attribs);
}

void bugle_glwin_context_create_free(struct glwin_context_create *create)
{
    bugle_free(create);
}

glwin_context bugle_glwin_get_context_destroy(function_call *call)
{
    return *call->eglDestroyContext.arg1;
}

void bugle_glwin_filter_catches_create_context(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "eglCreateContext", inactive, callback);
}

void bugle_glwin_filter_catches_destroy_context(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "eglDestroyContext", inactive, callback);
}

void bugle_glwin_filter_catches_make_current(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "eglMakeCurrent", inactive, callback);
}

void bugle_glwin_filter_catches_swap_buffers(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "eglSwapBuffers", inactive, callback);
}

void bugle_function_address_initialise_extra(void)
{
    size_t i;

    for (i = 0; i < budgie_function_count(); i++)
    {
        /* gengl.perl puts the OpenGL versions first, and ordered by version
         * number.
         * FIXME: should be made independent of client API.
         */
        if (bugle_api_function_extension(i) > BUGLE_API_EXTENSION_ID(GL_ES_VERSION_2_0))
        {
            BUDGIEAPIPROC ptr = bugle_glwin_get_proc_address(budgie_function_name(i));
            if (ptr != NULL)
                budgie_function_address_set_real(i, ptr);
        }
    }
}
