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
#include <stdbool.h>
#include <stddef.h>
#include <GL/gl.h>
#include <string.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/tracker.h>
#include <bugle/glutils.h>
#include <bugle/hashtable.h>
#include <budgie/call.h>
#include "lock.h"

object_class bugle_displaylist_class;
gl_lock_define_initialized(static, displaylist_lock);
static object_view displaylist_view;
static object_view namespace_view;   /* handle of the hash table */

typedef struct
{
    GLuint list;
    GLenum mode;
} displaylist_struct;

static void namespace_init(const void *key, void *data)
{
    bugle_hashptr_init((hashptr_table *) data, free);
}

static void namespace_clear(void *data)
{
    bugle_hashptr_clear((hashptr_table *) data);
}

static void displaylist_struct_init(const void *key, void *data)
{
    memcpy(data, key, sizeof(displaylist_struct));
}

GLenum bugle_displaylist_mode(void)
{
    displaylist_struct *info;

    info = bugle_object_get_current_data(&bugle_displaylist_class, displaylist_view);
    if (!info) return GL_NONE;
    else return info->mode;
}

GLuint bugle_displaylist_list(void)
{
    displaylist_struct *info;

    info = bugle_object_get_current_data(&bugle_displaylist_class, displaylist_view);
    if (!info) return 0;
    else return info->list;
}

void *bugle_displaylist_get(GLuint list)
{
    void *ans = NULL;
    hashptr_table *objects;

    gl_lock_lock(displaylist_lock);
    objects = bugle_object_get_current_data(&bugle_namespace_class, namespace_view);
    if (objects) ans = bugle_hashptr_get(objects, (void *) (size_t) list);
    gl_lock_unlock(displaylist_lock);
    return ans;
}

static bool trackdisplaylist_glNewList(function_call *call, const callback_data *data)
{
    displaylist_struct info;
    object *obj;
    GLint value;

    if (bugle_displaylist_list()) return true; /* Nested call */
    if (bugle_begin_internal_render())
    {
        CALL_glGetIntegerv(GL_LIST_INDEX, &value);
        info.list = value;
        CALL_glGetIntegerv(GL_LIST_MODE, &value);
        info.mode = value;
        if (info.list == 0) return true; /* Call failed? */
        obj = bugle_object_new(&bugle_displaylist_class, &info, true);
        bugle_end_internal_render("trackdisplaylist_callback", true);
    }
    return true;
}

static bool trackdisplaylist_glEndList(function_call *call, const callback_data *data)
{
    object *obj;
    displaylist_struct *info_ptr;
    hashptr_table *objects;

    obj = bugle_object_get_current(&bugle_displaylist_class);
    info_ptr = bugle_object_get_data(obj, displaylist_view);
    /* Note: we update the hash table when we end the list, since this is when OpenGL
     * says the new name comes into effect.
     */
    gl_lock_lock(displaylist_lock);
    objects = bugle_object_get_current_data(&bugle_namespace_class, namespace_view);
    if (objects)
        bugle_hashptr_set(objects, (void *) (size_t) info_ptr->list, obj);
    gl_lock_unlock(displaylist_lock);
    bugle_object_set_current(&bugle_displaylist_class, NULL);
    return true;
}

static bool trackdisplaylist_filter_set_initialise(filter_set *handle)
{
    filter *f;

    bugle_object_class_init(&bugle_displaylist_class, &bugle_context_class);

    f = bugle_filter_register(handle, "trackdisplaylist");
    bugle_filter_order("invoke", "trackdisplaylist");
    bugle_filter_catches(f, "glNewList", true, trackdisplaylist_glNewList);
    bugle_filter_catches(f, "glEndList", true, trackdisplaylist_glEndList);

    displaylist_view = bugle_object_view_register(&bugle_displaylist_class,
                                                   displaylist_struct_init,
                                                   NULL,
                                                   sizeof(displaylist_struct));
    namespace_view = bugle_object_view_register(&bugle_namespace_class,
                                                 namespace_init,
                                                 namespace_clear,
                                                 sizeof(hashptr_table));
    return true;
}

void trackdisplaylist_initialise(void)
{
    static const filter_set_info trackdisplaylist_info =
    {
        "trackdisplaylist",
        trackdisplaylist_filter_set_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* No documentation */
    };

    bugle_filter_set_register(&trackdisplaylist_info);

    bugle_filter_set_depends("trackdisplaylist", "trackcontext");
}
