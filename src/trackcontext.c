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
#include "src/canon.h"
#include "src/tracker.h"
#include "src/objects.h"
#include "budgielib/state.h"
#include "common/bool.h"
#include "common/hashtable.h"
#include <assert.h>
#include <stddef.h>
#include <GL/glx.h>
#if HAVE_PTHREAD_H
# include <pthread.h>
#endif

object_class bugle_context_class;
static bugle_hashptr_table context_objects;
static size_t trackcontext_offset;

state_7context_I *bugle_tracker_get_context_state()
{
    state_7context_I **cur;

    cur = (state_7context_I **) bugle_object_get_current_data(&bugle_context_class, trackcontext_offset);
    if (cur) return *cur;
    else return NULL;
}

static void tracker_set_context_state(state_7context_I *state)
{
    state_7context_I **cur;

    cur = (state_7context_I **) bugle_object_get_current_data(&bugle_context_class, trackcontext_offset);
    assert(cur);
    *cur = state;
}

static void trackcontext_initialise_state(const void *key, void *data)
{
    GLXContext ctx;
    state_generic *parent;

    parent = &budgie_get_root_state()->c_context.generic;
    ctx = (GLXContext) key;
    *(state_7context_I **) data = (state_7context_I *) budgie_get_state_index(parent, &ctx);
}

static bool trackcontext_callback(function_call *call, const callback_data *data)
{
    GLXContext ctx;
    state_generic *parent;
    state_7context_I *state;
    static pthread_mutex_t context_mutex = PTHREAD_MUTEX_INITIALIZER;
    void *obj;

    /* These calls may fail, so we must explicitly check for the
     * current context.
     */
    ctx = CALL_glXGetCurrentContext();
    if (!ctx)
        bugle_object_set_current(&bugle_context_class, NULL);
    else
    {
        parent = &budgie_get_root_state()->c_context.generic;
        pthread_mutex_lock(&context_mutex);
        if (!(state = (state_7context_I *) budgie_get_state_index(parent, &ctx)))
        {
            state = (state_7context_I *) budgie_add_state_index(parent, &ctx, NULL);
            obj = bugle_object_new(&bugle_context_class, ctx, true);
            bugle_hashptr_set(&context_objects, ctx, obj);
        }
        else
        {
            obj = bugle_hashptr_get(&context_objects, ctx);
            bugle_object_set_current(&bugle_context_class, obj);
            tracker_set_context_state(state);
        }
        pthread_mutex_unlock(&context_mutex);
    }
    return true;
}

static bool initialise_trackcontext(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "trackcontext");
    bugle_register_filter_depends("trackcontext", "invoke");
    bugle_register_filter_catches(f, FUNC_glXMakeCurrent, trackcontext_callback);
#ifdef FUNC_glXMakeContextCurrent
    bugle_register_filter_catches(f, FUNC_glXMakeContextCurrent, trackcontext_callback);
#endif
    trackcontext_offset = bugle_object_class_register(&bugle_context_class,
                                                      trackcontext_initialise_state,
                                                      NULL,
                                                      sizeof(state_7context_I *));
    return true;
}

void trackcontext_initialise(void)
{
    const filter_set_info trackcontext_info =
    {
        "trackcontext",
        initialise_trackcontext,
        NULL,
        NULL,
        0
    };

    bugle_object_class_init(&bugle_context_class, NULL);
    bugle_hashptr_init(&context_objects);
    bugle_register_filter_set(&trackcontext_info);
}
