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
#include "src/tracker.h"
#include "src/utils.h"
#include "common/bool.h"
#include "common/hashtable.h"
#include "common/threads.h"
#include "common/safemem.h"

typedef struct
{
    bugle_thread_mutex_t mutex;
    bugle_hashptr_table objects[BUGLE_TRACKOBJECTS_COUNT];
} trackobjects_data;

typedef struct
{
    budgie_group gen;
    budgie_group del;
    budgie_group bind;
    budgie_function is;
    bool shared;
} trackobjects_info;

/* FIXME: ARB_occlusion_query specifies that queries are NOT shared between contexts */
static const trackobjects_info info_table[BUGLE_TRACKOBJECTS_COUNT] =
{
    { GROUP_glGenTextures, GROUP_glDeleteTextures, GROUP_glBindTexture, FUNC_glIsTexture, true },

#ifdef GL_ARB_vertex_buffer_object
    { GROUP_glGenBuffersARB, GROUP_glDeleteBuffersARB, GROUP_glBindBufferARB, FUNC_glIsBufferARB, true },
#else
    { NULL_GROUP, NULL_GROUP, NULL_GROUP, NULL_FUNCTION },
#endif

#ifdef GL_ARB_occlusion_query
    { GROUP_glGenQueriesARB, GROUP_glDeleteQueriesARB, GROUP_glBeginQueryARB, FUNC_glIsQueryARB, false }
#else
    { NULL_GROUP, NULL_GROUP, NULL_GROUP, NULL_FUNCTION }
#endif
};

static bugle_object_view view;

static bugle_hashptr_table *get_table(budgie_group group)
{
    trackobjects_data *data;
    bugle_trackobjects_type type;

    data = bugle_object_get_current_data(&bugle_namespace_class, view);
    if (!data) return NULL;
    for (type = 0; type < BUGLE_TRACKOBJECTS_COUNT; type++)
    {
        if (group == info_table[type].gen
            || group == info_table[type].del
            || group == info_table[type].bind) return &data->objects[type];
    }
    abort();
}

static inline void lock(void)
{
    bugle_thread_mutex_lock(&((trackobjects_data *) bugle_object_get_current_data(&bugle_namespace_class, view))->mutex);
}

static inline void unlock(void)
{
    bugle_thread_mutex_unlock(&((trackobjects_data *) bugle_object_get_current_data(&bugle_namespace_class, view))->mutex);
}

static bool trackobjects_bind(function_call *call, const callback_data *data)
{
    GLenum target;
    GLuint object;
    bugle_hashptr_table *table;

    target = *(GLenum *) call->generic.args[0];
    object = *(GLuint *) call->generic.args[1];
    table = get_table(call->generic.group);

    if (table && object != 0)
    {
        lock();
        bugle_hashptr_set(table, (void *) (size_t) object, (void *) (size_t) target);
        unlock();
    }
    return true;
}

static bool trackobjects_delete(function_call *call, const callback_data *data)
{
    /* FIXME: no way to clear objects from the tracker */
    return true;
}

static void initialise_objects(const void *key, void *data)
{
    trackobjects_data *d;
    size_t i;

    d = (trackobjects_data *) data;
    bugle_thread_mutex_init(&d->mutex, NULL);
    for (i = 0; i < BUGLE_TRACKOBJECTS_COUNT; i++)
        bugle_hashptr_init(&d->objects[i], false);
}

static void destroy_objects(void *data)
{
    trackobjects_data *d;
    size_t i;

    d = (trackobjects_data *) data;

    for (i = 0; i < BUGLE_TRACKOBJECTS_COUNT; i++)
        bugle_hashptr_clear(&d->objects[i]);
}

static bool initialise_trackobjects(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "trackobjects");
    bugle_register_filter_catches(f, GROUP_glBindTexture, trackobjects_bind);
    bugle_register_filter_catches(f, GROUP_glDeleteTextures, trackobjects_delete);
#ifdef GL_ARB_vertex_buffer_object
    bugle_register_filter_catches(f, GROUP_glBindBufferARB, trackobjects_bind);
    bugle_register_filter_catches(f, GROUP_glDeleteBuffersARB, trackobjects_delete);
#endif
#ifdef GL_ARB_occlusion_query
    bugle_register_filter_catches(f, GROUP_glBeginQueryARB, trackobjects_bind);
    bugle_register_filter_catches(f, GROUP_glDeleteQueriesARB, trackobjects_delete);
#endif
    view = bugle_object_class_register(&bugle_namespace_class,
                                       initialise_objects,
                                       destroy_objects,
                                       sizeof(trackobjects_data));
    return true;
}

void bugle_trackobjects_get(bugle_trackobjects_type type,
                            bugle_linked_list *ids)
{
    trackobjects_data *data;
    bugle_hashptr_table *table;
    const bugle_hashptr_entry *i;
    GLboolean (*is)(GLuint);
    bugle_trackobjects_id *ptr;
    GLuint id;

    bugle_list_init(ids, true);
    data = bugle_object_get_current_data(&bugle_namespace_class, view);
    if (!data) return;
    table = &data->objects[type];
    is = (GLboolean (*)(GLuint)) budgie_function_table[info_table[type].is].real;
    if (!is) return;

    lock();
    for (i = bugle_hashptr_begin(table); i; i = bugle_hashptr_next(table, i))
    {
        id = (GLuint) (size_t) i->key;
        if ((*is)(id))
        {
            ptr = bugle_malloc(sizeof(bugle_trackobjects_id));
            ptr->id = id;
            ptr->target = (GLenum) (size_t) i->value;
            bugle_list_append(ids, ptr);
        }
    }
    unlock();
}

void trackobjects_initialise(void)
{
    static const filter_set_info trackobjects_info =
    {
        "trackobjects",
        initialise_trackobjects,
        NULL,
        NULL,
        0,
        NULL /* no documentation */
    };

    bugle_register_filter_set(&trackobjects_info);
    bugle_register_filter_set_depends("trackobject", "trackcontext");
}
