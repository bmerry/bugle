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

#ifndef BUGLE_SRC_LINKEDLIST_H
#define BUGLE_SRC_LINKEDLIST_H
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "common/bool.h"

typedef struct list_node_s
{
    void *data;
    struct list_node_s *prev;
    struct list_node_s *next;
} list_node;

typedef struct
{
    list_node *head;
    list_node *tail;
} linked_list;

void list_init(linked_list *l);
void *list_data(const list_node *node);
void list_set_data(list_node *node, void *data);
list_node *list_prepend(linked_list *l, void *data);
list_node *list_append(linked_list *l, void *data);
list_node *list_insert_before(linked_list *l, list_node *node, void *data);
list_node *list_insert_after(linked_list *l, list_node *node, void *data);
list_node *list_head(const linked_list *l);
list_node *list_tail(const linked_list *l);
list_node *list_next(const list_node *node);
list_node *list_prev(const list_node *node);
void list_erase(linked_list *l, list_node *node, bool free_data);
void list_clear(linked_list *l, bool free_data);

#endif /* !BUGLE_SRC_LINKEDLIST_H */
