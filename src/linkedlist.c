#include <stdlib.h>
#include "linkedlist.h"
#if HAVE_CONFIG_H
# include <config.h>
#endif

void list_init(linked_list *l)
{
    l->head = l->tail = NULL;
}

void *list_data(list_node *node)
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

    n = (list_node *) malloc(sizeof(list_node));
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
    n = (list_node *) malloc(sizeof(list_node));
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
    n = (list_node *) malloc(sizeof(list_node));
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
    n = (list_node *) malloc(sizeof(list_node));
    n->data = data;
    n->next = node->next;
    n->next->prev = n;
    n->prev = node;
    node->next = n;
    return n;
}

list_node *list_head(linked_list *l)
{
    return l->head;
}

list_node *list_tail(linked_list *l)
{
    return l->tail;
}

list_node *list_next(list_node *node)
{
    return node->next;
}

list_node *list_prev(list_node *node)
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
