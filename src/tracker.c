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
#include "common/safemem.h"
#include "common/hashtable.h"
#include <assert.h>
#include <stddef.h>
#include <GL/glx.h>
#if HAVE_PTHREAD_H
# include <pthread.h>
#endif

object_class context_class;
static hashptr_table context_objects;
static size_t trackcontext_offset, trackbeginend_offset;

state_7context_I *tracker_get_context_state()
{
    state_7context_I **cur;

    cur = object_get_current(&context_class, trackcontext_offset);
    if (cur) return *cur;
    else return NULL;
}

static void tracker_set_context_state(void *obj, state_7context_I *state)
{
    state_7context_I **data;

    data = (state_7context_I **) (((char *) obj) + trackcontext_offset);
    *data = state;
}

static void trackcontext_initialise_state(const void *key, void *data)
{
    GLXContext ctx;
    state_generic *parent;

    parent = &get_root_state()->c_context.generic;
    ctx = (GLXContext) key;
    *(state_7context_I **) data = (state_7context_I *) get_state_index(parent, &ctx);
}

static bool trackcontext_callback(function_call *call, const callback_data *data)
{
    GLXContext ctx;
    state_generic *parent;
    state_7context_I *state;
    static pthread_mutex_t context_mutex = PTHREAD_MUTEX_INITIALIZER;
    void *obj;

    switch (canonical_call(call))
    {
    case CFUNC_glXMakeCurrent:
#ifdef GLX_VERSION_1_3
    case CFUNC_glXMakeContextCurrent:
#endif
        /* These calls may fail, so we must explicitly check for the
         * current context.
         */
        ctx = glXGetCurrentContext();
        if (!ctx)
            object_set_current(&context_class, NULL);
        else
        {
            parent = &get_root_state()->c_context.generic;
            pthread_mutex_lock(&context_mutex);
            if (!(state = (state_7context_I *) get_state_index(parent, &ctx)))
            {
                state = (state_7context_I *) add_state_index(parent, &ctx, NULL);
                obj = object_new(&context_class, ctx, true);
                hashptr_set(&context_objects, ctx, obj);
            }
            else
            {
                obj = hashptr_get(&context_objects, ctx);
                tracker_set_context_state(obj, state);
                object_set_current(&context_class, obj);
            }
            pthread_mutex_unlock(&context_mutex);
        }
        break;
    }
    return true;
}

/* Track whether we are in glBegin/glEnd. This is trickier than it looks,
 * because glBegin can fail if given an illegal primitive, and we can't
 * check for the error because glGetError is illegal inside glBegin/glEnd.
 */

static bool trackbeginend_callback(function_call *call, const callback_data *data)
{
    bool *begin_end;

    switch (canonical_call(call))
    {
    case CFUNC_glBegin:
        switch (*call->typed.glBegin.arg0)
        {
        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_QUADS:
        case GL_QUAD_STRIP:
        case GL_POLYGON:
            begin_end = (bool *) object_get_current(&context_class, trackbeginend_offset);
            if (begin_end != NULL) *begin_end = true;
        default: ;
        }
        break;
    case CFUNC_glEnd:
        begin_end = (bool *) object_get_current(&context_class, trackbeginend_offset);
        if (begin_end != NULL) *begin_end = false;
        break;
    }
    return true;
}

bool in_begin_end(void)
{
    bool *begin_end;

    begin_end = (bool *) object_get_current(&context_class, trackbeginend_offset);
    return !begin_end || *begin_end;
}

static bool initialise_trackcontext(filter_set *handle)
{
    filter *f;

    f = register_filter(handle, "trackcontext", trackcontext_callback);
    register_filter_depends("trackcontext", "invoke");
    register_filter_catches(f, FUNC_glXMakeCurrent);
#ifdef FUNC_glXMakeContextCurrent
    register_filter_catches(f, FUNC_glXMakeContextCurrent);
#endif
    return true;
}

static bool initialise_trackbeginend(filter_set *handle)
{
    filter *f;

    f = register_filter(handle, "trackbeginend", trackbeginend_callback);
    register_filter_depends("trackbeginend", "invoke");
    register_filter_depends("trackbeginend", "trackcontext");
    register_filter_catches(f, FUNC_glBegin);
    register_filter_catches(f, FUNC_glEnd);
    register_filter_set_depends("trackbeginend", "trackcontext");
    return true;
}

void tracker_initialise(void)
{
    const filter_set_info trackcontext_info =
    {
        "trackcontext",
        initialise_trackcontext,
        NULL,
        NULL,
        0
    };
    const filter_set_info trackbeginend_info =
    {
        "trackbeginend",
        initialise_trackbeginend,
        NULL,
        NULL,
        0
    };

    object_class_init(&context_class);
    hashptr_init(&context_objects);
    /* These ought to be in the initialise routines, but it is vital that
     * they run first and we currently have no other way to determine the
     * ordering.
     */
    trackcontext_offset = object_class_register(&context_class,
                                                trackcontext_initialise_state,
                                                NULL,
                                                sizeof(state_7context_I *));
    trackbeginend_offset = object_class_register(&context_class,
                                                 NULL,
                                                 NULL,
                                                 sizeof(bool));
    register_filter_set(&trackcontext_info);
    register_filter_set(&trackbeginend_info);
}
