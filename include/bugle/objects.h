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
#include <stdbool.h>
#include <bugle/linkedlist.h>

typedef size_t object_view;
typedef struct object_class object_class;
typedef struct object object;

object_class *bugle_object_class_new(object_class *parent);
void bugle_object_class_free(object_class *klass);
/* Returns an offset into the structure, which should be passed back to
 * object_get_current to get the data associated with this registration.
 * The key passed to the structure is determined by the individual classes,
 * and may give more information about the abstract object.
 */
object_view bugle_object_view_new(object_class *klass,
                                  void (*constructor)(const void *key, void *data),
                                  void (*destructor)(void *data),
                                  size_t size);
object *    bugle_object_new(object_class *klass, const void *key, bool make_current);
void        bugle_object_free(object *obj);
object *    bugle_object_get_current(const object_class *klass);
void *      bugle_object_get_current_data(const object_class *klass, object_view view);
void        bugle_object_set_current(object_class *klass, object *obj);
void *      bugle_object_get_data(object *obj, object_view view);

#endif /* !BUGLE_SRC_OBJECTS_H */
