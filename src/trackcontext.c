/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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
#include "src/filters.h"
#include "src/utils.h"
#include "src/tracker.h"
#include "src/objects.h"
#include "common/bool.h"
#include "common/hashtable.h"
#include "common/threads.h"
#include "common/safemem.h"
#include <assert.h>
#include <stddef.h>
#include <GL/glx.h>

/* Some constructors like the context to be current when they are run.
 * To facilitate this, the object is not constructed until the first
 * use of the context. However, some of the useful information is passed
 * as parameters to glXCreate[New]Context, so they are put in
 * a trackcontext_data struct in the initial_values hash.
 */

bugle_object_class bugle_context_class;
bugle_object_class bugle_namespace_class;
static bugle_hashptr_table context_objects, namespace_objects;
static bugle_hashptr_table initial_values;
static bugle_object_view trackcontext_view;
static bugle_thread_mutex_t context_mutex = BUGLE_THREAD_MUTEX_INITIALIZER;

typedef struct
{
    GLXContext root_context;  /* context that owns the namespace - possibly self*/
    GLXContext aux_context;
    XVisualInfo visual_info;
    bool use_visual_info;
} trackcontext_data;

static bool trackcontext_newcontext(function_call *call, const callback_data *data)
{
    GLXContext self, parent;
    trackcontext_data *base, *up;

    switch (call->generic.group)
    {
#ifdef GLX_VERSION_1_3
    case GROUP_glXCreateNewContext:
        self = *call->typed.glXCreateNewContext.retn;
        parent = *call->typed.glXCreateNewContext.arg3;
        break;
#endif
    case GROUP_glXCreateContext:
        self = *call->typed.glXCreateContext.retn;
        parent = *call->typed.glXCreateContext.arg2;
        break;
    default:
        abort();
    }

    if (self)
    {
        bugle_thread_mutex_lock(&context_mutex);

        base = (trackcontext_data *) bugle_malloc(sizeof(trackcontext_data));
        base->aux_context = NULL;
        if (parent)
        {
            up = (trackcontext_data *) bugle_hashptr_get(&initial_values, parent);
            if (!up)
            {
                fprintf(stderr, "CRITICAL: share context %p unknown\n", (void *) parent);
                base->root_context = self;
            }
            else
                base->root_context = up->root_context;
        }
        switch (call->generic.group)
        {
#ifdef GLX_VERSION_1_3
        case GROUP_glXCreateNewContext:
            base->use_visual_info = false;
            break;
#endif
        case GROUP_glXCreateContext:
            base->visual_info = **call->typed.glXCreateContext.arg1;
            base->use_visual_info = true;
            break;
        default:
            abort();
        }

        bugle_hashptr_set(&initial_values, self, base);
        bugle_thread_mutex_unlock(&context_mutex);
    }

    return true;
}

static bool trackcontext_callback(function_call *call, const callback_data *data)
{
    GLXContext ctx;
    bugle_object *obj, *ns;
    trackcontext_data *initial, *view;

    /* These calls may fail, so we must explicitly check for the
     * current context.
     */
    ctx = CALL_glXGetCurrentContext();
    if (!ctx)
        bugle_object_set_current(&bugle_context_class, NULL);
    else
    {
        bugle_thread_mutex_lock(&context_mutex);
        obj = bugle_hashptr_get(&context_objects, ctx);
        if (!obj)
        {
            obj = bugle_object_new(&bugle_context_class, ctx, true);
            bugle_hashptr_set(&context_objects, ctx, obj);
            initial = bugle_hashptr_get(&initial_values, ctx);
            if (initial == NULL)
            {
                fprintf(stderr, "CRITICAL: context %p used but not created\n",
                        (void *) ctx);
            }
            else
            {
                view = bugle_object_get_data(&bugle_context_class, obj, trackcontext_view);
                *view = *initial;
                ns = bugle_hashptr_get(&namespace_objects, view->root_context);
                if (!ns)
                {
                    ns = bugle_object_new(&bugle_namespace_class, ctx, true);
                    bugle_hashptr_set(&namespace_objects, ctx, ns);
                }
                else
                    bugle_object_set_current(&bugle_namespace_class, ns);
            }
        }
        else
            bugle_object_set_current(&bugle_context_class, obj);
        bugle_thread_mutex_unlock(&context_mutex);
    }
    return true;
}

static bool initialise_trackcontext(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "trackcontext");
    bugle_register_filter_depends("trackcontext", "invoke");
    bugle_register_filter_catches(f, GROUP_glXMakeCurrent, true, trackcontext_callback);
    bugle_register_filter_catches(f, GROUP_glXCreateContext, true, trackcontext_newcontext);
#ifdef GLX_VERSION_1_3
    bugle_register_filter_catches(f, GROUP_glXMakeContextCurrent, true, trackcontext_callback);
    bugle_register_filter_catches(f, GROUP_glXCreateNewContext, true, trackcontext_newcontext);
#endif
    trackcontext_view = bugle_object_class_register(&bugle_context_class,
                                                    NULL,
                                                    NULL,
                                                    sizeof(trackcontext_data));
    return true;
}

GLXContext bugle_get_aux_context()
{
    trackcontext_data *data;
    GLXContext old_ctx, ctx;
    int render_type = 0, screen;
    int n;
    int attribs[3] = {GLX_FBCONFIG_ID, 0, None};
    GLXFBConfig *cfgs;
    Display *dpy;
    int major = -1, minor = -1;

    data = bugle_object_get_current_data(&bugle_context_class, trackcontext_view);
    if (!data) return NULL; /* no current context, hence no aux context */
    if (data->aux_context == NULL)
    {
        dpy = CALL_glXGetCurrentDisplay();
        old_ctx = CALL_glXGetCurrentContext();
        CALL_glXQueryVersion(dpy, &major, &minor);

#ifdef GLX_VERSION_1_3
        if (major >= 1 && (major > 1 || minor >= 3))
        {
            /* Have all the facilities to extract the necessary information */
            CALL_glXQueryContext(dpy, old_ctx, GLX_RENDER_TYPE_SGIX, &render_type);
            CALL_glXQueryContext(dpy, old_ctx, GLX_SCREEN_EXT, &screen);
            CALL_glXQueryContext(dpy, old_ctx, GLX_FBCONFIG_ID_SGIX, &attribs[1]);
            /* It seems that render_type comes back as a boolean, although
             * the spec seems to indicate that it should be otherwise.
             */
            if (render_type <= 1)
                render_type = render_type ? GLX_RGBA_TYPE_SGIX : GLX_COLOR_INDEX_TYPE_SGIX;
            cfgs = CALL_glXChooseFBConfig(dpy, screen, attribs, &n);
            if (cfgs == NULL)
            {
                fprintf(stderr, "Warning: could not create an auxiliary context: no matching FBConfig\n");
                return NULL;
            }
            ctx = CALL_glXCreateNewContext(dpy, cfgs[0], render_type,
                                           old_ctx, CALL_glXIsDirect(dpy, old_ctx));
            XFree(cfgs);
            if (ctx == NULL)
                fprintf(stderr, "Warning: could not create an auxiliary context: creation failed\n");
        }
        else
#endif
        {
            if (data->use_visual_info)
            {
                ctx = CALL_glXCreateContext(dpy, &data->visual_info, old_ctx,
                                            CALL_glXIsDirect(dpy, old_ctx));
                if (ctx == NULL)
                    fprintf(stderr, "Warning: could not create an auxiliary context: creation failed\n");
            }
            else
            {
                fprintf(stderr, "Warning: could not create an auxiliary context: missing extensions\n");
                return NULL;
            }
        }
        data->aux_context = ctx;
    }
    return data->aux_context;
}

void trackcontext_initialise(void)
{
    static const filter_set_info trackcontext_info =
    {
        "trackcontext",
        initialise_trackcontext,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        NULL /* No documentation */
    };

    bugle_object_class_init(&bugle_context_class, NULL);
    bugle_object_class_init(&bugle_namespace_class, &bugle_context_class);
    bugle_hashptr_init(&context_objects, true);
    bugle_hashptr_init(&namespace_objects, true);
    bugle_hashptr_init(&initial_values, true);
    bugle_atexit((void (*)(void *)) bugle_hashptr_clear, &context_objects);
    bugle_atexit((void (*)(void *)) bugle_hashptr_clear, &namespace_objects);
    bugle_atexit((void (*)(void *)) bugle_hashptr_clear, &initial_values);
    bugle_register_filter_set(&trackcontext_info);
}
