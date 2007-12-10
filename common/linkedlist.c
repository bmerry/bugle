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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "linkedlist.h"
#include "common/safemem.h"
#include "xalloc.h"

void bugle_list_init(linked_list *l, bool owns_memory)
{
    l->head = l->tail = NULL;
    l->owns_memory = owns_memory;
}

void *bugle_list_data(const linked_list_node *node)
{
    return node->data;
}

void bugle_list_set_data(linked_list_node *node, void *data)
{
    node->data = data;
}

linked_list_node *bugle_list_prepend(linked_list *l, void *data)
{
    linked_list_node *n;

    n = XMALLOC(linked_list_node);
    n->prev = NULL;
    n->next = l->head;
    n->data = data;
    if (l->head) l->head->prev = n;
    l->head = n;
    if (!l->tail) l->tail = n;
    return n;
}

linked_list_node *bugle_list_append(linked_list *l, void *data)
{
    linked_list_node *n;
    n = XMALLOC(linked_list_node);
    n->next = NULL;
    n->prev = l->tail;
    n->data = data;
    if (l->tail) l->tail->next = n;
    l->tail = n;
    if (!l->head) l->head = n;
    return n;
}

linked_list_node *bugle_list_insert_before(linked_list *l,
                                          linked_list_node *node,
                                          void *data)
{
    linked_list_node *n;

    if (node == l->head) return bugle_list_prepend(l, data);
    n = XMALLOC(linked_list_node);
    n->data = data;
    n->prev = node->prev;
    n->prev->next = n;
    n->next = node;
    node->prev = n;
    return n;
}

linked_list_node *bugle_list_insert_after(linked_list *l,
                                         linked_list_node *node,
                                         void *data)
{
    linked_list_node *n;

    if (node == l->tail) return bugle_list_append(l, data);
    n = XMALLOC(linked_list_node);
    n->data = data;
    n->next = node->next;
    n->next->prev = n;
    n->prev = node;
    node->next = n;
    return n;
}

linked_list_node *bugle_list_head(const linked_list *l)
{
    return l->head;
}

linked_list_node *bugle_list_tail(const linked_list *l)
{
    return l->tail;
}

linked_list_node *bugle_list_next(const linked_list_node *node)
{
    return node->next;
}

linked_list_node *bugle_list_prev(const linked_list_node *node)
{
    return node->prev;
}

void bugle_list_erase(linked_list *l, linked_list_node *node)
{
    if (l->owns_memory) free(node->data);

    if (node->next) node->next->prev = node->prev;
    else l->tail = node->prev;

    if (node->prev) node->prev->next = node->next;
    else l->head = node->next;

    free(node);
}

void bugle_list_clear(linked_list *l)
{
    linked_list_node *cur, *nxt;

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
