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
void *list_data(list_node *node);
void list_set_data(list_node *node, void *data);
list_node *list_prepend(linked_list *l, void *data);
list_node *list_append(linked_list *l, void *data);
list_node *list_insert_before(linked_list *l, list_node *node, void *data);
list_node *list_insert_after(linked_list *l, list_node *node, void *data);
list_node *list_head(linked_list *l);
list_node *list_tail(linked_list *l);
list_node *list_next(list_node *node);
list_node *list_prev(list_node *node);
void list_erase(linked_list *l, list_node *node, bool free_data);
void list_clear(linked_list *l, bool free_data);

#endif /* !BUGLE_SRC_LINKEDLIST_H */
