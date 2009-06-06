/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2008  Bruce Merry
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
#include <bugle/bool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/gldisplaylist.h>
#include <bugle/gl/globjects.h>
#include <bugle/gl/glutils.h>
#include <bugle/hashtable.h>
#include <budgie/call.h>
#include "platform/threads.h"

object_class *bugle_displaylist_class;

#if GL_VERSION_1_1
bugle_thread_lock_define_initialized(static, displaylist_lock)
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

    info = bugle_object_get_current_data(bugle_displaylist_class, displaylist_view);
    if (!info) return GL_NONE;
    else return info->mode;
}

GLuint bugle_displaylist_list(void)
{
    displaylist_struct *info;

    info = bugle_object_get_current_data(bugle_displaylist_class, displaylist_view);
    if (!info) return 0;
    else return info->list;
}

void *bugle_displaylist_get(GLuint list)
{
    void *ans = NULL;
    hashptr_table *objects;

    bugle_thread_lock_lock(displaylist_lock);
    objects = bugle_object_get_current_data(bugle_namespace_class, namespace_view);
    if (objects) ans = bugle_hashptr_get(objects, (void *) (size_t) list);
    bugle_thread_lock_unlock(displaylist_lock);
    return ans;
}

static bugle_bool gldisplaylist_glNewList(function_call *call, const callback_data *data)
{
    displaylist_struct info;
    object *obj;
    GLint value;

    if (bugle_displaylist_list()) return BUGLE_TRUE; /* Nested call */
    if (bugle_gl_begin_internal_render())
    {
        CALL(glGetIntegerv)(GL_LIST_INDEX, &value);
        info.list = value;
        CALL(glGetIntegerv)(GL_LIST_MODE, &value);
        info.mode = value;
        if (info.list == 0) return BUGLE_TRUE; /* Call failed? */
        obj = bugle_object_new(bugle_displaylist_class, &info, BUGLE_TRUE);
        bugle_gl_end_internal_render("gldisplaylist_callback", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool gldisplaylist_glEndList(function_call *call, const callback_data *data)
{
    object *obj;
    displaylist_struct *info_ptr;
    hashptr_table *objects;

    obj = bugle_object_get_current(bugle_displaylist_class);
    info_ptr = bugle_object_get_data(obj, displaylist_view);
    /* Note: we update the hash table when we end the list, since this is when OpenGL
     * says the new name comes into effect.
     */
    bugle_thread_lock_lock(displaylist_lock);
    objects = bugle_object_get_current_data(bugle_namespace_class, namespace_view);
    if (objects)
        bugle_hashptr_set(objects, (void *) (size_t) info_ptr->list, obj);
    bugle_thread_lock_unlock(displaylist_lock);
    bugle_object_set_current(bugle_displaylist_class, NULL);
    return BUGLE_TRUE;
}
#else /* !GL_VERSION_1_1 */
GLenum bugle_displaylist_mode(void)
{
    return GL_NONE;
}

GLuint bugle_displaylist_list(void)
{
    return 0;
}
#endif

static bugle_bool gldisplaylist_filter_set_initialise(filter_set *handle)
{
    filter *f;

    bugle_displaylist_class = bugle_object_class_new(bugle_context_class);

#if GL_VERSION_1_1
    f = bugle_filter_new(handle, "gldisplaylist");
    bugle_filter_order("invoke", "gldisplaylist");
    bugle_filter_catches(f, "glNewList", BUGLE_TRUE, gldisplaylist_glNewList);
    bugle_filter_catches(f, "glEndList", BUGLE_TRUE, gldisplaylist_glEndList);

    displaylist_view = bugle_object_view_new(bugle_displaylist_class,
                                             displaylist_struct_init,
                                             NULL,
                                             sizeof(displaylist_struct));
    namespace_view = bugle_object_view_new(bugle_namespace_class,
                                           namespace_init,
                                           namespace_clear,
                                           sizeof(hashptr_table));
#endif
    return BUGLE_TRUE;
}

void gldisplaylist_initialise(void)
{
    static const filter_set_info gldisplaylist_info =
    {
        "gldisplaylist",
        gldisplaylist_filter_set_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* No documentation */
    };

    bugle_filter_set_new(&gldisplaylist_info);

    bugle_filter_set_depends("gldisplaylist", "trackcontext");
}
