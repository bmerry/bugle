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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include "linkedlist.h"
#include "common/safemem.h"

void list_init(linked_list *l)
{
    l->head = l->tail = NULL;
}

void *list_data(const list_node *node)
{
    return node->data;
}

void list_set_data(list_node *node, void *data)
{
    node->data = data;
}

list_node *list_prepend(linked_list *l, void *data)
{
    list_node *n;

    n = (list_node *) xmalloc(sizeof(list_node));
    n->prev = NULL;
    n->next = l->head;
    n->data = data;
    if (l->head) l->head->prev = n;
    l->head = n;
    if (!l->tail) l->tail = n;
    return n;
}

list_node *list_append(linked_list *l, void *data)
{
    list_node *n;
    n = (list_node *) xmalloc(sizeof(list_node));
    n->next = NULL;
    n->prev = l->tail;
    n->data = data;
    if (l->tail) l->tail->next = n;
    l->tail = n;
    if (!l->head) l->head = n;
    return n;
}

list_node *list_insert_before(linked_list *l, list_node *node, void *data)
{
    list_node *n;

    if (node == l->head) return list_prepend(l, data);
    n = (list_node *) xmalloc(sizeof(list_node));
    n->data = data;
    n->prev = node->prev;
    n->prev->next = n;
    n->next = node;
    node->prev = n;
    return n;
}

list_node *list_insert_after(linked_list *l, list_node *node, void *data)
{
    list_node *n;

    if (node == l->tail) return list_append(l, data);
    n = (list_node *) xmalloc(sizeof(list_node));
    n->data = data;
    n->next = node->next;
    n->next->prev = n;
    n->prev = node;
    node->next = n;
    return n;
}

list_node *list_head(const linked_list *l)
{
    return l->head;
}

list_node *list_tail(const linked_list *l)
{
    return l->tail;
}

list_node *list_next(const list_node *node)
{
    return node->next;
}

list_node *list_prev(const list_node *node)
{
    return node->prev;
}

void list_erase(linked_list *l, list_node *node, bool free_data)
{
    if (free_data) free(node->data);

    if (node->next) node->next->prev = node->prev;
    else l->tail = node->prev;

    if (node->prev) node->prev->next = node->next;
    else l->head = node->next;

    free(node);
}

void list_clear(linked_list *l, bool free_data)
{
    list_node *cur, *nxt;

    cur = l->head;
    while (cur)
    {
        nxt = cur->next;
        if (free_data) free(cur->data);
        free(cur);
        cur = nxt;
    }
    l->head = l->tail = NULL;
}
