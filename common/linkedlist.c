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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include "linkedlist.h"
#include "common/safemem.h"

void bugle_list_init(bugle_linked_list *l, bool owns_memory)
{
    l->head = l->tail = NULL;
    l->owns_memory = owns_memory;
}

void *bugle_list_data(const bugle_list_node *node)
{
    return node->data;
}

void bugle_list_set_data(bugle_list_node *node, void *data)
{
    node->data = data;
}

bugle_list_node *bugle_list_prepend(bugle_linked_list *l, void *data)
{
    bugle_list_node *n;

    n = (bugle_list_node *) bugle_malloc(sizeof(bugle_list_node));
    n->prev = NULL;
    n->next = l->head;
    n->data = data;
    if (l->head) l->head->prev = n;
    l->head = n;
    if (!l->tail) l->tail = n;
    return n;
}

bugle_list_node *bugle_list_append(bugle_linked_list *l, void *data)
{
    bugle_list_node *n;
    n = (bugle_list_node *) bugle_malloc(sizeof(bugle_list_node));
    n->next = NULL;
    n->prev = l->tail;
    n->data = data;
    if (l->tail) l->tail->next = n;
    l->tail = n;
    if (!l->head) l->head = n;
    return n;
}

bugle_list_node *bugle_list_insert_before(bugle_linked_list *l,
                                          bugle_list_node *node,
                                          void *data)
{
    bugle_list_node *n;

    if (node == l->head) return bugle_list_prepend(l, data);
    n = (bugle_list_node *) bugle_malloc(sizeof(bugle_list_node));
    n->data = data;
    n->prev = node->prev;
    n->prev->next = n;
    n->next = node;
    node->prev = n;
    return n;
}

bugle_list_node *bugle_list_insert_after(bugle_linked_list *l,
                                         bugle_list_node *node,
                                         void *data)
{
    bugle_list_node *n;

    if (node == l->tail) return bugle_list_append(l, data);
    n = (bugle_list_node *) bugle_malloc(sizeof(bugle_list_node));
    n->data = data;
    n->next = node->next;
    n->next->prev = n;
    n->prev = node;
    node->next = n;
    return n;
}

bugle_list_node *bugle_list_head(const bugle_linked_list *l)
{
    return l->head;
}

bugle_list_node *bugle_list_tail(const bugle_linked_list *l)
{
    return l->tail;
}

bugle_list_node *bugle_list_next(const bugle_list_node *node)
{
    return node->next;
}

bugle_list_node *bugle_list_prev(const bugle_list_node *node)
{
    return node->prev;
}

void bugle_list_erase(bugle_linked_list *l, bugle_list_node *node)
{
    if (l->owns_memory) free(node->data);

    if (node->next) node->next->prev = node->prev;
    else l->tail = node->prev;

    if (node->prev) node->prev->next = node->next;
    else l->head = node->next;

    free(node);
}

void bugle_list_clear(bugle_linked_list *l)
{
    bugle_list_node *cur, *nxt;

    cur = l->head;
    while (cur)
    {
        nxt = cur->next;
        if (l->owns_memory) free(cur->data);
        free(cur);
        cur = nxt;
    }
    l->head = l->tail = NULL;
}
