/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008, 2011, 2013  Bruce Merry
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
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/glwintypes.h>
#include <bugle/filters.h>
#include <bugle/apireflect.h>
#include <budgie/call.h>
#include <X11/Xlib.h>
#include <bugle/memory.h>
#include <bugle/bool.h>
#include <stdlib.h>
#include <string.h>
#include "budgielib/defines.h"

/* Wrappers around GLX/WGL/EGL functions */
glwin_display bugle_glwin_get_current_display(void)
{
    return CALL(glXGetCurrentDisplay)();
}

glwin_context bugle_glwin_get_current_context(void)
{
    return CALL(glXGetCurrentContext)();
}

glwin_drawable bugle_glwin_get_current_drawable(void)
{
    return CALL(glXGetCurrentDrawable)();
}

glwin_drawable bugle_glwin_get_current_read_drawable(void)
{
    return CALL(glXGetCurrentReadDrawableSGI)();
}

bugle_bool bugle_glwin_make_context_current(glwin_display dpy, glwin_drawable draw,
                                      glwin_drawable read, glwin_context ctx)
{
    return CALL(glXMakeCurrentReadSGI)(dpy, draw, read, ctx);
}

BUDGIEAPIPROC bugle_glwin_get_proc_address(const char *name)
{
    return CALL(glXGetProcAddressARB)((const GLubyte *) name);
}

void bugle_glwin_query_version(glwin_display dpy, int *major, int *minor)
{
    CALL(glXQueryVersion)(dpy, major, minor);
}

const char *bugle_glwin_query_extensions_string(glwin_display dpy)
{
    /* FIXME: take a screen number, or find out a default somehow */
    return CALL(glXQueryExtensionsString)(dpy, 0);
}

void bugle_glwin_get_drawable_dimensions(glwin_display dpy, glwin_drawable drawable,
                                         int *width, int *height)
{
    unsigned int uwidth, uheight;
#ifdef GLX_VERSION_1_3
    int major, minor;

    glXQueryVersion(dpy, &major, &minor);
    if (major > 1 || (major == 1 && minor >= 3))
    {
        glXQueryDrawable(dpy, drawable, GLX_WIDTH, &uwidth);
        glXQueryDrawable(dpy, drawable, GLX_HEIGHT, &uheight);
    }
#endif
    else
    {
        /* Ok, can get tricky if we don't have 1.3, since no extension defined
         * a glXQueryDrawable??? extension. Just hope it is a window and not
         * a pbuffer or pixmap.
         * FIXME: should track this at creation time. OTOH, GLX 1.3 is has been
         * around since 1998 old and vendors really ought to support it by now.
         */
        Window root;
        int x, y;
        unsigned int border, depth;

        XGetGeometry(dpy, drawable, &root, &x, &y,
                     &uwidth, &uheight, &border, &depth);
    }
    *width = uwidth;
    *height = uheight;
}

typedef struct glwin_context_create_glx
{
    glwin_context_create parent;
    XVisualInfo visual_info;
    GLXFBConfigSGIX config;
    int render_type;
    Bool direct;
    int *attribs;
} glwin_context_create_glx;

glwin_context_create *bugle_glwin_context_create_save(function_call *call)
{
    glwin_context ctx;
    glwin_context_create_glx *create;

    ctx = *(glwin_context *) call->generic.retn;
    if (!ctx)
        return NULL;
    create = BUGLE_ZALLOC(glwin_context_create_glx);
    create->parent.function = call->generic.id;
    create->parent.group = call->generic.group;
    create->parent.ctx = ctx;

    switch (call->generic.group)
    {
#ifdef GLX_ARB_create_context
    case GROUP_glXCreateContextAttribsARB:
        create->parent.dpy =   *call->glXCreateContextAttribsARB.arg0;
        create->config =       *call->glXCreateContextAttribsARB.arg1;
        create->parent.share = *call->glXCreateContextAttribsARB.arg2;
        create->direct =       *call->glXCreateContextAttribsARB.arg3;
        {
            const int *orig_attribs = *call->glXCreateContextAttribsARB.arg4;
            if (orig_attribs == NULL)
                create->attribs = NULL;
            else
            {
                int n_attribs = bugle_count_glwin_attributes(orig_attribs, None);
                create->attribs = BUGLE_NMALLOC(n_attribs, int);
                memcpy(create->attribs, orig_attribs, n_attribs * sizeof(int));
            }
        }
        break;
#endif /* GLX_ARB_create_context */
    case GROUP_glXCreateContextWithConfigSGIX:
        create->parent.dpy =   *call->glXCreateContextWithConfigSGIX.arg0;
        create->config =       *call->glXCreateContextWithConfigSGIX.arg1;
        create->render_type =  *call->glXCreateContextWithConfigSGIX.arg2;
        create->parent.share = *call->glXCreateContextWithConfigSGIX.arg3;
        create->direct =       *call->glXCreateContextWithConfigSGIX.arg4;
        break;
    case GROUP_glXCreateContext:
        create->parent.dpy =   *call->glXCreateContext.arg0;
        create->visual_info = **call->glXCreateContext.arg1;
        create->parent.share = *call->glXCreateContext.arg2;
        create->direct =       *call->glXCreateContext.arg3;
        ctx = *call->glXCreateContext.retn;
        break;
    default:
        abort(); /* Should never happen */
    }

    return (glwin_context_create *) create;
}

glwin_context bugle_glwin_context_create_new(const glwin_context_create *create, bugle_bool share)
{
    const glwin_context_create_glx *create_glx;
    function_call new_call;
    glwin_context retn;
    glwin_context share_ctx;
    const XVisualInfo *vi;

    create_glx = (glwin_context_create_glx *) create;
    share_ctx = share ? create_glx->parent.ctx : NULL;

    /* By constructing a call object, we are sure to call the same function
     * as the original call, even if it is through GLX 1.3 rather than
     * an extension or vice versa
     */
    new_call.generic.id = create_glx->parent.function;
    new_call.generic.group = create_glx->parent.group;
    new_call.generic.user_data = NULL;
    new_call.generic.retn = &retn;

    /* The explicit typecasts below are to discard constness. */
    switch (create_glx->parent.group)
    {
#if GLX_ARB_create_context
    case GROUP_glXCreateContextAttribsARB:
        new_call.generic.num_args = 5;
        new_call.glXCreateContextAttribsARB.arg0 = (Display **) &create_glx->parent.dpy;
        new_call.glXCreateContextAttribsARB.arg1 = (GLXFBConfigSGIX *) &create_glx->config;
        new_call.glXCreateContextAttribsARB.arg2 = (GLXContext *) &share_ctx;
        new_call.glXCreateContextAttribsARB.arg3 = (Bool *) &create_glx->direct;
        new_call.glXCreateContextAttribsARB.arg4 = (const int **) &create_glx->attribs;
        break;
#endif /* GLX_ARB_create_context */
    case GROUP_glXCreateContextWithConfigSGIX:
        new_call.generic.num_args = 5;
        new_call.glXCreateContextWithConfigSGIX.arg0 = (Display **) &create_glx->parent.dpy;
        new_call.glXCreateContextWithConfigSGIX.arg1 = (GLXFBConfigSGIX *) &create_glx->config;
        new_call.glXCreateContextWithConfigSGIX.arg2 = (int *) &create_glx->render_type;
        new_call.glXCreateContextWithConfigSGIX.arg3 = (GLXContext *) &share_ctx;
        new_call.glXCreateContextWithConfigSGIX.arg4 = (Bool *) &create_glx->direct;
        break;
    case GROUP_glXCreateContext:
        new_call.generic.num_args = 4;
        new_call.glXCreateContext.arg0 = (Display **) &create_glx->parent.dpy;
        vi = &create_glx->visual_info;
        new_call.glXCreateContext.arg1 = (XVisualInfo **) &vi;
        new_call.glXCreateContext.arg2 = (GLXContext *) &share_ctx;
        new_call.glXCreateContext.arg3 = (Bool *) &create_glx->direct;
        break;
    default:
        abort(); /* Should never happen */
    }

    budgie_invoke(&new_call);
    return retn;
}

void bugle_glwin_context_create_free(struct glwin_context_create *create)
{
    const glwin_context_create_glx *create_glx = (glwin_context_create_glx *) create;
    bugle_free(create_glx->attribs);
    bugle_free(create);
}

glwin_context bugle_glwin_get_context_destroy(function_call *call)
{
    return *call->glXDestroyContext.arg1;
}

void bugle_glwin_filter_catches_create_context(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "glXCreateContext", inactive, callback);
    bugle_filter_catches(f, "glXCreateContextWithConfigSGIX", inactive, callback);
    bugle_filter_catches(f, "glXCreateContextAttribsARB", inactive, callback);
}

void bugle_glwin_filter_catches_destroy_context(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "glXDestroyContext", inactive, callback);
}

void bugle_glwin_filter_catches_make_current(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "glXMakeCurrent", inactive, callback);
    bugle_filter_catches(f, "glXMakeCurrentReadSGI", inactive, callback);
}

void bugle_glwin_filter_catches_swap_buffers(filter *f, bugle_bool inactive, filter_callback callback)
{
    bugle_filter_catches(f, "glXSwapBuffers", inactive, callback);
}

/* The Linux ABI requires that OpenGL 1.2 functions be accessible by
 * direct dynamic linking, but everything else should be accessed by
 * glXGetProcAddressARB. We deal with that here. However, we don't
 * use glXGetProcAddressARB to query itself.
 */
void bugle_function_address_initialise_extra(void)
{
    budgie_function i;

    for (i = 0; i < budgie_function_count(); i++)
    {
        /* gengl.py puts the OpenGL versions first, and ordered by decreasing version
         * number.
         */
        if (bugle_api_function_extension(i) < BUGLE_API_EXTENSION_ID(GL_VERSION_1_2)
            || bugle_api_function_extension(i) > BUGLE_API_EXTENSION_ID(GL_VERSION_1_1))
        {
            BUDGIEAPIPROC ptr = bugle_glwin_get_proc_address(budgie_function_name(i));
            if (ptr != NULL && ptr != budgie_function_address_wrapper(i))
                budgie_function_address_set_real(i, ptr);
        }
    }
}
