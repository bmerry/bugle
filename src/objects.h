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

#ifndef BUGLE_SRC_OBJECTS_H
#define BUGLE_SRC_OBJECTS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include "common/linkedlist.h"
#include "common/bool.h"
#include "common/threads.h"

typedef size_t object_view;

typedef struct object_class_s
{
    size_t count;       /* number of registrants */
    linked_list info;   /* list of object_class_info, defined in objects.c */
    bugle_thread_key_t current;

    struct object_class_s *parent;
    object_view parent_view; /* view where we store current of this class in parent */
} object_class;

typedef struct
{
    const object_class *klass;
    size_t count;
    void *views[1]; /* actually [count] at runtime */
} object;

void bugle_object_class_init(object_class *klass, object_class *parent);
void bugle_object_class_clear(object_class *klass);
/* Returns an offset into the structure, which should be passed back to
 * object_get_current to get the data associated with this registration.
 * The key passed to the structure is determined by the individual classes,
 * and may give more information about the abstract object.
 */
object_view bugle_object_view_register(object_class *klass,
                                       void (*constructor)(const void *key, void *data),
                                       void (*destructor)(void *data),
                                       size_t size);
object *    bugle_object_new(const object_class *klass, const void *key, bool make_current);
void        bugle_object_destroy(object *obj);
object *    bugle_object_get_current(const object_class *klass);
void *      bugle_object_get_current_data(const object_class *klass, object_view view);
void        bugle_object_set_current(const object_class *klass, object *obj);
void *      bugle_object_get_data(object *obj, object_view view);

#endif /* !BUGLE_SRC_OBJECTS_H */
