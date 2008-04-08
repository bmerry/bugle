/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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
#define GLX_GLXEXT_PROTOTYPES
#include <stdbool.h>
#include <bugle/hashtable.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <bugle/filters.h>
#include <bugle/tracker.h>
#include <bugle/objects.h>
#include <bugle/log.h>
#include <budgie/types.h>
#include <budgie/addresses.h>
#include "budgielib/defines.h"
#include "src/glx/glxtables.h"
#include "xalloc.h"
#include "lock.h"

/* Some constructors like the context to be current when they are run.
 * To facilitate this, the object is not constructed until the first
 * use of the context. However, some of the useful information is passed
 * as parameters to glXCreate[New]Context, so they are put in
 * a trackcontext_data struct in the initial_values hash.
 */

object_class *bugle_context_class;
object_class *bugle_namespace_class;
static hashptr_table context_objects, namespace_objects;
static hashptr_table initial_values;
static object_view trackcontext_view;
gl_lock_define_initialized(static, context_mutex)

typedef struct
{
    Display *dpy;
    GLXContext root_context;  /* context that owns the namespace - possibly self */
    GLXContext aux_shared;
    GLXContext aux_unshared;
    XVisualInfo visual_info;
    bool use_visual_info;
} trackcontext_data;

static bool trackcontext_newcontext(function_call *call, const callback_data *data)
{
    Display *dpy;
    GLXContext self, parent;
    trackcontext_data *base, *up;

    switch (call->generic.group)
    {
#ifdef GLX_SGIX_fbconfig
    case GROUP_glXCreateContextWithConfigSGIX:
        dpy = *call->glXCreateContextWithConfigSGIX.arg0;
        self = *call->glXCreateContextWithConfigSGIX.retn;
        parent = *call->glXCreateContextWithConfigSGIX.arg3;
        break;
#endif
    case GROUP_glXCreateContext:
        dpy = *call->glXCreateContext.arg0;
        self = *call->glXCreateContext.retn;
        parent = *call->glXCreateContext.arg2;
        break;
    default:
        abort();
    }

    if (self)
    {
        gl_lock_lock(context_mutex);

        base = XZALLOC(trackcontext_data);
        base->dpy = dpy;
        base->aux_shared = NULL;
        base->aux_unshared = NULL;
        if (parent)
        {
            up = (trackcontext_data *) bugle_hashptr_get(&initial_values, parent);
            if (!up)
            {
                bugle_log_printf("trackcontext", "newcontext", BUGLE_LOG_WARNING,
                                 "share context %p unknown", (void *) parent);
                base->root_context = self;
            }
            else
                base->root_context = up->root_context;
        }
        switch (call->generic.group)
        {
#ifdef GLX_SGIX_fbconfig
        case GROUP_glXCreateContextWithConfigSGIX:
            base->use_visual_info = false;
            break;
#endif
        case GROUP_glXCreateContext:
            base->visual_info = **call->glXCreateContext.arg1;
            base->use_visual_info = true;
            break;
        default:
            abort();
        }

        bugle_hashptr_set(&initial_values, self, base);
        gl_lock_unlock(context_mutex);
    }

    return true;
}

static bool trackcontext_callback(function_call *call, const callback_data *data)
{
    GLXContext ctx;
    object *obj, *ns;
    trackcontext_data *initial, *view;

    /* These calls may fail, so we must explicitly check for the
     * current context.
     */
    ctx = CALL(glXGetCurrentContext)();
    if (!ctx)
        bugle_object_set_current(bugle_context_class, NULL);
    else
    {
        gl_lock_lock(context_mutex);
        obj = bugle_hashptr_get(&context_objects, ctx);
        if (!obj)
        {
            obj = bugle_object_new(bugle_context_class, ctx, true);
            bugle_hashptr_set(&context_objects, ctx, obj);
            initial = bugle_hashptr_get(&initial_values, ctx);
            if (initial == NULL)
            {
                bugle_log_printf("trackcontext", "makecurrent", BUGLE_LOG_WARNING,
                                 "context %p used but not created",
                                 (void *) ctx);
            }
            else
            {
                view = bugle_object_get_data(obj, trackcontext_view);
                *view = *initial;
                ns = bugle_hashptr_get(&namespace_objects, view->root_context);
                if (!ns)
                {
                    ns = bugle_object_new(bugle_namespace_class, ctx, true);
                    bugle_hashptr_set(&namespace_objects, ctx, ns);
                }
                else
                    bugle_object_set_current(bugle_namespace_class, ns);
            }
        }
        else
            bugle_object_set_current(bugle_context_class, obj);
        gl_lock_unlock(context_mutex);
    }
    return true;
}

static void trackcontext_data_clear(void *data)
{
#if 0
    trackcontext_data *d;

    d = (trackcontext_data *) data;
    /* This is disabled for now because it causes problems when the
     * X connection is shut down but the context is never explicitly
     * destroyed. SDL does this.
     *
     * Unfortunately, this means that an application that creates and
     * explicitly destroys many contexts will leak aux contexts.
     */
    if (d->aux_shared) CALL(glXDestroyContext)(d->dpy, d->aux_shared);
    if (d->aux_unshared) CALL(glXDestroyContext)(d->dpy, d->aux_unshared);
#endif
}

static bool trackcontext_filter_set_initialise(filter_set *handle)
{
    filter *f;

    bugle_context_class = bugle_object_class_new(NULL);
    bugle_namespace_class = bugle_object_class_new(bugle_context_class);
    bugle_hashptr_init(&context_objects, (void (*)(void *)) bugle_object_free);
    bugle_hashptr_init(&namespace_objects, (void (*)(void *)) bugle_object_free);
    bugle_hashptr_init(&initial_values, free);

    f = bugle_filter_new(handle, "trackcontext");
    bugle_filter_order("invoke", "trackcontext");
    bugle_filter_catches(f, "glXMakeCurrent", true, trackcontext_callback);
    bugle_filter_catches(f, "glXCreateContext", true, trackcontext_newcontext);
#ifdef GLX_SGI_make_current_read
    bugle_filter_catches(f, "glXMakeCurrentReadSGI", true, trackcontext_callback);
#endif
#ifdef GLX_SGIX_fbconfig
    bugle_filter_catches(f, "glXCreateContextWithConfigSGIX", true, trackcontext_newcontext);
#endif
    trackcontext_view = bugle_object_view_new(bugle_context_class,
                                              NULL,
                                              trackcontext_data_clear,
                                              sizeof(trackcontext_data));
    return true;
}

static void trackcontext_filter_set_shutdown(filter_set *handle)
{
    bugle_hashptr_clear(&context_objects);
    bugle_hashptr_clear(&namespace_objects);
    bugle_hashptr_clear(&initial_values);
    bugle_object_class_free(bugle_namespace_class);
    bugle_object_class_free(bugle_context_class);
}

GLXContext bugle_get_aux_context(bool shared)
{
    trackcontext_data *data;
    GLXContext old_ctx, ctx;
    GLXContext *aux;
    int render_type = 0, screen;
    int n;
    int attribs[3] = {GLX_FBCONFIG_ID, 0, None};
    GLXFBConfig *cfgs;
    Display *dpy;
    int major = -1, minor = -1;

    data = bugle_object_get_current_data(bugle_context_class, trackcontext_view);
    if (!data) return NULL; /* no current context, hence no aux context */
    aux = shared ? &data->aux_shared : &data->aux_unshared;
    if (*aux == NULL)
    {
        dpy = CALL(glXGetCurrentDisplay)();
        old_ctx = CALL(glXGetCurrentContext)();
        CALL(glXQueryVersion)(dpy, &major, &minor);

#ifdef GLX_VERSION_1_3
        if (major >= 1 && (major > 1 || minor >= 3))
        {
            /* Have all the facilities to extract the necessary information */
            CALL(glXQueryContext)(dpy, old_ctx, GLX_RENDER_TYPE, &render_type);
            CALL(glXQueryContext)(dpy, old_ctx, GLX_SCREEN, &screen);
            CALL(glXQueryContext)(dpy, old_ctx, GLX_FBCONFIG_ID, &attribs[1]);
            /* I haven't quite figured out the return values, but I'm guessing
             * that it is returning one of the GLX_RGBA_BIT values rather than
             * GL_RGBA_TYPE etc.
             */
            if (render_type == GLX_RGBA_BIT)
                render_type = GLX_RGBA_TYPE;
            else if (render_type == GLX_COLOR_INDEX_BIT)
                render_type = GLX_COLOR_INDEX_TYPE;
#ifdef GLX_ARB_fbconfig_float
            else if (render_type == GLX_RGBA_FLOAT_BIT_ARB)
                render_type = GLX_RGBA_FLOAT_TYPE_ARB;
#endif
            cfgs = CALL(glXChooseFBConfig)(dpy, screen, attribs, &n);
            if (cfgs == NULL)
            {
                bugle_log("trackcontext", "aux", BUGLE_LOG_WARNING,
                          "could not create an auxiliary context: no matching FBConfig");
                return NULL;
            }
            ctx = CALL(glXCreateNewContext)(dpy, cfgs[0], render_type,
                                           shared ? old_ctx : NULL,
                                           CALL(glXIsDirect)(dpy, old_ctx));
            XFree(cfgs);
            if (ctx == NULL)
                bugle_log("trackcontext", "aux", BUGLE_LOG_WARNING,
                          "could not create an auxiliary context: creation failed");
        }
        else
#endif
        {
            if (data->use_visual_info)
            {
                ctx = CALL(glXCreateContext)(dpy, &data->visual_info,
                                            shared ? old_ctx : NULL,
                                            CALL(glXIsDirect)(dpy, old_ctx));
                if (!ctx)
                {
                    bugle_log("trackcontext", "aux", BUGLE_LOG_WARNING,
                              "could not create an auxiliary context: creation failed");
                    return NULL;
                }
            }
            else
            {
                bugle_log("trackcontext", "aux", BUGLE_LOG_WARNING,
                          "could not create an auxiliary context: missing GLX extensions");
                return NULL;
            }
        }
        *aux = ctx;
    }
    return *aux;
}

Display *bugle_get_current_display_internal(bool lock)
{
    trackcontext_data *data;
    GLXContext ctx;

    ctx = CALL(glXGetCurrentContext)();
    if (!ctx)
        return NULL;
    if (lock) gl_lock_lock(context_mutex);
    data = (trackcontext_data *) bugle_hashptr_get(&initial_values, ctx);
    if (lock) gl_lock_unlock(context_mutex);
    if (data)
        return data->dpy;
#ifdef GLX_EXT_import_context
    else if (CALL(glXGetCurrentDisplayEXT))
        return CALL(glXGetCurrentDisplayEXT)();
#endif
#ifdef GLX_VERSION_1_3
    else if (CALL(glXGetCurrentDisplay))
        return CALL(glXGetCurrentDisplay)();
#endif
    return NULL;
}

Display *bugle_get_current_display(void)
{
    return bugle_get_current_display_internal(true);
}

GLXDrawable bugle_get_current_read_drawable(void)
{
#ifdef GLX_VERSION_1_3
    if (bugle_gl_has_extension(BUGLE_GLX_VERSION_1_3))
        return CALL(glXGetCurrentReadDrawable)();
#endif
#ifdef GLX_SGI_make_current_read
    if (bugle_gl_has_extension(BUGLE_GLX_SGI_make_current_read))
        return CALL(glXGetCurrentReadDrawableSGI)();
#endif
    return CALL(glXGetCurrentDrawable)();
}

Bool bugle_make_context_current(Display *dpy, GLXDrawable draw,
                                GLXDrawable read, GLXContext ctx)
{
    /* FIXME: should depend on the capabilities of the target context */
#ifdef GLX_VERSION_1_3
    if (bugle_gl_has_extension(BUGLE_GLX_VERSION_1_3))
        return CALL(glXMakeContextCurrent)(dpy, draw, read, ctx);
#endif
#ifdef GLX_SGI_make_current_read
    if (bugle_gl_has_extension(BUGLE_GLX_SGI_make_current_read))
        return CALL(glXMakeCurrentReadSGI)(dpy, draw, read, ctx);
#endif
    return CALL(glXMakeCurrent)(dpy, draw, ctx);
}

void trackcontext_initialise(void)
{
    static const filter_set_info trackcontext_info =
    {
        "trackcontext",
        trackcontext_filter_set_initialise,
        trackcontext_filter_set_shutdown,
        NULL,
        NULL,
        NULL,
        NULL /* No documentation */
    };

    bugle_filter_set_new(&trackcontext_info);
}
