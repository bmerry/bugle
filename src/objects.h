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

/* This is not an OOP layer, like gobject. It provides a way for filters
 * to attach storage to various _abstract_ objects, such as contexts,
 * display lists, or glBegin/glEnd blocks.
 *
 * These is a notion of a "current" object in each class. This is either
 * per-thread, or else is slaved to another class. When slaved, there is
 * a current object within each parent object, and object_get_current will
 * return the current object within the current parent (recursively).
 *
 * FIXME: the current memory packing scheme can cause misalignments. This
 * should be ok for x86/amd64 because they allow misaligned accesses, but
 * could cause crashes on other architectures.
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

typedef struct object_class_s
{
    linked_list info;
    size_t total_size;
    pthread_key_t current;

    struct object_class_s *parent;
    size_t parent_offset;
} object_class;

void object_class_init(object_class *klass, object_class *parent);
void object_class_clear(object_class *klass);
/* Returns an offset into the structure, which should be passed back to
 * object_get_current to get the data associated with this registration.
 * The key passed to the structure is determined by the individual classes,
 * and may give more information about the abstract object.
 */
size_t object_class_register(object_class *klass,
                             void (*constructor)(const void *key, void *data),
                             void (*destructor)(void *data),
                             size_t size);
void *object_new(const object_class *klass, const void *key, bool make_current);
void object_delete(const object_class *klass, void *obj);
void *object_get_current(const object_class *klass);
void *object_get_current_data(const object_class *klass, size_t offset);
void object_set_current(const object_class *klass, void *obj);
void *object_get_data(const object_class *klass, void *obj, size_t offset);

#endif /* !BUGLE_SRC_OBJECTS_H */
