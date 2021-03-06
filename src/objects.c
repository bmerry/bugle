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

/* This is not an OOP layer, like gobject. See doc/objects.html for an
 * explanation.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/linkedlist.h>
#include <bugle/objects.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "platform/threads.h"

struct object_class
{
    size_t count;       /* number of registrants */
    linked_list info;   /* list of object_class_info, defined in objects.c */
    bugle_thread_key_t current;

    struct object_class *parent;
    object_view parent_view; /* view where we store current of this class in parent */
};

struct object
{
    object_class *klass;
    size_t count;
    void *views[1]; /* actually [count] at runtime */
};

typedef struct
{
    void (*constructor)(const void *key, void *data);
    void (*destructor)(void *data);
    size_t size;
} object_class_info;

object_class * bugle_object_class_new(object_class *parent)
{
    object_class *klass;

    klass = BUGLE_MALLOC(object_class);
    bugle_list_init(&klass->info, bugle_free);
    klass->parent = parent;
    klass->count = 0;
    if (parent)
        klass->parent_view = bugle_object_view_new(parent, NULL, NULL, sizeof(object *));
    else
        bugle_thread_key_create(&klass->current, NULL);
    return klass;
}

void bugle_object_class_free(object_class *klass)
{
    bugle_list_clear(&klass->info);
    if (!klass->parent)
        bugle_thread_key_delete(klass->current);
    bugle_free(klass);
}

object_view bugle_object_view_new(object_class *klass,
                                  void (*constructor)(const void *key, void *data),
                                  void (*destructor)(void *data),
                                  size_t size)
{
    object_class_info *info;

    info = BUGLE_MALLOC(object_class_info);
    info->constructor = constructor;
    info->destructor = destructor;
    info->size = size;
    bugle_list_append(&klass->info, info);
    return klass->count++;
}

object *bugle_object_new(object_class *klass, const void *key, bugle_bool make_current)
{
    object *obj;
    linked_list_node *i;
    const object_class_info *info;
    size_t j;

    obj = bugle_malloc(sizeof(object) + klass->count * sizeof(void *) - sizeof(void *));
    obj->klass = klass;
    obj->count = klass->count;

    for (i = bugle_list_head(&klass->info), j = 0; i; i = bugle_list_next(i), j++)
    {
        info = (const object_class_info *) bugle_list_data(i);
        if (info->size)
        {
            obj->views[j] = bugle_malloc(info->size);
            memset(obj->views[j], 0, info->size);
        }
        else
            obj->views[j] = NULL;
    }

    if (make_current) bugle_object_set_current(klass, obj);

    for (i = bugle_list_head(&klass->info), j = 0; i; i = bugle_list_next(i), j++)
    {
        info = (const object_class_info *) bugle_list_data(i);
        if (info->constructor)
            (*info->constructor)(key, obj->views[j]);
    }
    return obj;
}

void bugle_object_free(object *obj)
{
    linked_list_node *i;
    size_t j;
    const object_class_info *info;

    if (!obj) return;
    if (obj == bugle_object_get_current(obj->klass))
        bugle_object_set_current(obj->klass, NULL);
    for (i = bugle_list_head(&obj->klass->info), j = 0; i; i = bugle_list_next(i), j++)
    {
        info = (const object_class_info *) bugle_list_data(i);
        if (info->destructor)
            (*info->destructor)(obj->views[j]);
        bugle_free(obj->views[j]);
    }
    bugle_free(obj);
}

object *bugle_object_get_current(const object_class *klass)
{
    void *ans;

    if (klass->parent)
    {
        ans = bugle_object_get_current_data(klass->parent, klass->parent_view);
        if (!ans) return NULL;
        else return *(object **) ans;
    }
    else
        return (object *) bugle_thread_getspecific(klass->current);
}

void *bugle_object_get_current_data(const object_class *klass, object_view view)
{
    return bugle_object_get_data(bugle_object_get_current(klass), view);
}

void bugle_object_set_current(object_class *klass, object *obj)
{
    void *tmp;

    if (klass->parent)
    {
        tmp = bugle_object_get_current_data(klass->parent, klass->parent_view);
        if (tmp) *(object **) tmp = obj;
    }
    else
        bugle_thread_setspecific(klass->current, (void *) obj);
}

void *bugle_object_get_data(object *obj, object_view view)
{
    if (!obj) return NULL;
    else return obj->views[view];
}
