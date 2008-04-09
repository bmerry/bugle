/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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
#include <stdbool.h>
#include <bugle/hashtable.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <bugle/glwin.h>
#include <bugle/filters.h>
#include <bugle/tracker.h>
#include <bugle/objects.h>
#include <bugle/log.h>
#include <budgie/types.h>
#include <budgie/addresses.h>
#include "budgielib/defines.h"
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
    glwin_context root_context;  /* context that owns the namespace - possibly self */
    glwin_context aux_shared;
    glwin_context aux_unshared;
    glwin_context_create *create;
} trackcontext_data;

static bool trackcontext_newcontext(function_call *call, const callback_data *data)
{
    trackcontext_data *base, *up;
    glwin_context_create *create;

    create = bugle_glwin_context_create_save(call);

    if (create)
    {
        gl_lock_lock(context_mutex);

        base = XZALLOC(trackcontext_data);
        base->aux_shared = NULL;
        base->aux_unshared = NULL;
        base->create = create;
        if (create->share)
        {
            up = (trackcontext_data *) bugle_hashptr_get(&initial_values, create->share);
            if (!up)
            {
                bugle_log_printf("trackcontext", "newcontext", BUGLE_LOG_WARNING,
                                 "share context %p unknown", (void *) create->share);
                base->root_context = create->ctx;
            }
            else
                base->root_context = up->root_context;
        }

        bugle_hashptr_set(&initial_values, create->ctx, base);
        gl_lock_unlock(context_mutex);
    }

    return true;
}

static bool trackcontext_callback(function_call *call, const callback_data *data)
{
    glwin_context ctx;
    object *obj, *ns;
    trackcontext_data *initial, *view;

    /* These calls may fail, so we must explicitly check for the
     * current context.
     */
    ctx = bugle_glwin_get_current_context();
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
    trackcontext_data *d;

    d = (trackcontext_data *) data;
#if 0
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
    free(d->create);
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
    bugle_glwin_filter_catches_make_current(f, true, trackcontext_callback);
    bugle_glwin_filter_catches_create_context(f, true, trackcontext_newcontext);
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

glwin_context bugle_get_aux_context(bool shared)
{
    trackcontext_data *data;
    glwin_context old_ctx, ctx;
    glwin_context *aux;

    data = bugle_object_get_current_data(bugle_context_class, trackcontext_view);
    if (!data) return NULL; /* no current context, hence no aux context */
    aux = shared ? &data->aux_shared : &data->aux_unshared;
    if (*aux == NULL)
    {
        old_ctx = bugle_glwin_get_current_context();

        ctx = bugle_glwin_context_create_new(data->create, shared);
        if (ctx == NULL)
            bugle_log("trackcontext", "aux", BUGLE_LOG_WARNING,
                      "could not create an auxiliary context: creation failed");
        *aux = ctx;
    }
    return *aux;
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
