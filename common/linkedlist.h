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

#ifndef BUGLE_COMMON_LINKEDLIST_H
#define BUGLE_COMMON_LINKEDLIST_H
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct linked_list_node_s
{
    void *data;
    struct linked_list_node_s *prev;
    struct linked_list_node_s *next;
} linked_list_node;

typedef struct
{
    linked_list_node *head;
    linked_list_node *tail;
    bool owns_memory;
} linked_list;

/* Fills in the data for a linked list, which must already have allocated
 * memory (in most cases, linked lists live on the stack or in global
 * memory). If owns_memory is true, bugle_list_erase and bugle_list_clear
 * will automatically call free() on the values.
 */
void bugle_list_init(linked_list *l, bool owns_memory);
void *bugle_list_data(const linked_list_node *node);
void bugle_list_set_data(linked_list_node *node, void *data);
linked_list_node *bugle_list_prepend(linked_list *l, void *data);
linked_list_node *bugle_list_append(linked_list *l, void *data);
linked_list_node *bugle_list_insert_before(linked_list *l, linked_list_node *node, void *data);
linked_list_node *bugle_list_insert_after(linked_list *l, linked_list_node *node, void *data);
linked_list_node *bugle_list_head(const linked_list *l);
linked_list_node *bugle_list_tail(const linked_list *l);
linked_list_node *bugle_list_next(const linked_list_node *node);
linked_list_node *bugle_list_prev(const linked_list_node *node);
void bugle_list_erase(linked_list *l, linked_list_node *node);
void bugle_list_clear(linked_list *l);

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_COMMON_LINKEDLIST_H */
