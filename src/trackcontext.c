/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

bugle_object_class bugle_context_class;
static bugle_hashptr_table context_objects;
static bugle_object_view trackcontext_view;

typedef struct
{
    GLXContext aux_context;
} trackcontext_data;

static bool trackcontext_callback(function_call *call, const callback_data *data)
{
    GLXContext ctx;
    static bugle_thread_mutex_t context_mutex = BUGLE_THREAD_MUTEX_INITIALIZER;
    bugle_object *obj;

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
    bugle_register_filter_catches(f, GROUP_glXMakeCurrent, trackcontext_callback);
#ifdef GLX_VERSION_1_3
    bugle_register_filter_catches(f, GROUP_glXMakeContextCurrent, trackcontext_callback);
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

    data = bugle_object_get_current_data(&bugle_context_class, trackcontext_view);
    if (!data) return NULL; // no current context, hence no aux context
    if (data->aux_context == NULL)
    {
        dpy = CALL_glXGetCurrentDisplay();
        old_ctx = CALL_glXGetCurrentContext();
        CALL_glXQueryContext(dpy, old_ctx, GLX_RENDER_TYPE, &render_type);
        CALL_glXQueryContext(dpy, old_ctx, GLX_SCREEN, &screen);
        CALL_glXQueryContext(dpy, old_ctx, GLX_FBCONFIG_ID, &attribs[1]);
        /* It seems that render_type comes back as a boolean, although
         * the spec seems to indicate that it should be otherwise.
         */
        if (render_type <= 1)
            render_type = render_type ? GLX_RGBA_TYPE : GLX_COLOR_INDEX_TYPE;
        cfgs = CALL_glXChooseFBConfig(dpy, screen, attribs, &n);
        if (cfgs == NULL)
        {
            fprintf(stderr, "Warning: could not create an auxiliary context\n");
            return NULL;
        }
        ctx = CALL_glXCreateNewContext(dpy, cfgs[0], render_type,
                                       old_ctx, CALL_glXIsDirect(dpy, old_ctx));
        XFree(cfgs);
        if (ctx == NULL)
            fprintf(stderr, "Warning: could not create an auxiliary context\n");
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
        0,
        NULL /* No documentation */
    };

    bugle_object_class_init(&bugle_context_class, NULL);
    bugle_hashptr_init(&context_objects, true);
    bugle_atexit((void (*)(void *)) bugle_hashptr_clear, &context_objects);
    bugle_register_filter_set(&trackcontext_info);
}
