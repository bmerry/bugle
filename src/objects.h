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

#ifndef BUGLE_SRC_OBJECTS_H
#define BUGLE_SRC_OBJECTS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include <pthread.h>
#include "common/linkedlist.h"
#include "common/bool.h"

typedef size_t bugle_object_view;

typedef struct object_class_s
{
    size_t count;     /* number of registrants */
    bugle_linked_list info;
    pthread_key_t current;

    struct object_class_s *parent;
    bugle_object_view parent_view; /* view where we store current of this class in parent */
} bugle_object_class;

typedef struct
{
    const bugle_object_class *klass;
    size_t count;
    void *views[1]; /* actually [count] at runtime */
} bugle_object;

void bugle_object_class_init(bugle_object_class *klass, bugle_object_class *parent);
void bugle_object_class_clear(bugle_object_class *klass);
/* Returns an offset into the structure, which should be passed back to
 * object_get_current to get the data associated with this registration.
 * The key passed to the structure is determined by the individual classes,
 * and may give more information about the abstract object.
 */
bugle_object_view bugle_object_class_register(bugle_object_class *klass,
                                              void (*constructor)(const void *key, void *data),
                                              void (*destructor)(void *data),
                                              size_t size);
bugle_object *bugle_object_new(const bugle_object_class *klass, const void *key, bool make_current);
void bugle_object_delete(const bugle_object_class *klass, bugle_object *obj);
bugle_object *bugle_object_get_current(const bugle_object_class *klass);
void *bugle_object_get_current_data(const bugle_object_class *klass, bugle_object_view view);
void bugle_object_set_current(const bugle_object_class *klass, bugle_object *obj);
void *bugle_object_get_data(const bugle_object_class *klass, bugle_object *obj, bugle_object_view view);

#endif /* !BUGLE_SRC_OBJECTS_H */
