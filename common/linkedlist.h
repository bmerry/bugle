/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
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

#ifndef BUGLE_COMMON_LINKEDLIST_H
#define BUGLE_COMMON_LINKEDLIST_H
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "common/bool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bugle_list_node_s
{
    void *data;
    struct bugle_list_node_s *prev;
    struct bugle_list_node_s *next;
} bugle_list_node;

typedef struct
{
    bugle_list_node *head;
    bugle_list_node *tail;
    bool owns_memory;
} bugle_linked_list;

void bugle_list_init(bugle_linked_list *l, bool owns_memory);
void *bugle_list_data(const bugle_list_node *node);
void bugle_list_set_data(bugle_list_node *node, void *data);
bugle_list_node *bugle_list_prepend(bugle_linked_list *l, void *data);
bugle_list_node *bugle_list_append(bugle_linked_list *l, void *data);
bugle_list_node *bugle_list_insert_before(bugle_linked_list *l, bugle_list_node *node, void *data);
bugle_list_node *bugle_list_insert_after(bugle_linked_list *l, bugle_list_node *node, void *data);
bugle_list_node *bugle_list_head(const bugle_linked_list *l);
bugle_list_node *bugle_list_tail(const bugle_linked_list *l);
bugle_list_node *bugle_list_next(const bugle_list_node *node);
bugle_list_node *bugle_list_prev(const bugle_list_node *node);
void bugle_list_erase(bugle_linked_list *l, bugle_list_node *node);
void bugle_list_clear(bugle_linked_list *l);

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_COMMON_LINKEDLIST_H */
