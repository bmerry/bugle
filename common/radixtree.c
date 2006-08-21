/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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
#include "common/radixtree.h"
#include "common/safemem.h"
#include "common/bool.h"

/* The presence or absence of leaf nodes indicates whether the
 * corresponding integers are in the radix tree.
 */
/* FIXME: set up a union to save memory */
typedef struct bugle_radix_node_t
{
    struct bugle_radix_node_t *left, *right;
    void *value;
} bugle_radix_node;

static inline bugle_radix_tree_type tree_max(int bits)
{
    /* We rely on wraparound for the case where bits is the number of
     * bits in the type itself.
     */
    return (((bugle_radix_tree_type) 1) << bits) - 1;
}

static inline bugle_radix_tree_type tree_bit(int bits)
{
    return ((bugle_radix_tree_type) 1) << (bits - 1);
}

void bugle_radix_tree_init(bugle_radix_tree *tree, bool owns_memory)
{
    tree->root = NULL;
    tree->owns_memory = owns_memory;
    tree->bits = 0;
}

static void radix_tree_set(bugle_radix_tree *tree, bugle_radix_tree_type key,
                           void *value)
{
    bugle_radix_node **cur, *n = NULL;
    bugle_radix_tree_type bit;
    int i;

    while (key > tree_max(tree->bits))
    {
        n = bugle_malloc(sizeof(bugle_radix_node));
        n->left = tree->root;
        n->right = NULL;
        n->value = NULL;
        tree->root = n;
        tree->bits++;
    }

    bit = tree_bit(tree->bits);
    cur = &tree->root;
    for (i = 0; i <= tree->bits; i++)
    {
        n = *cur;
        if (!n)
        {
            n = bugle_malloc(sizeof(bugle_radix_node));
            n->left = NULL;
            n->right = NULL;
            n->value = NULL;
            *cur = n;
        }
        if (key & bit) cur = &n->right;
        else cur = &n->left;

        bit >>= 1;
    }
    if (tree->owns_memory && n->value) free(n->value);
    n->value = value;
}

static void radix_tree_unset(bugle_radix_tree *tree, bugle_radix_tree_type key)
{
    bugle_radix_tree_type bit;
    bugle_radix_node **stack[sizeof(bugle_radix_tree_type) * CHAR_BIT + 1], **cur, *n = NULL;
    int i;

    if (key > tree_max(tree->bits)) return; /* Beyond the range of the tree */
    bit = tree_bit(tree->bits);
    cur = &tree->root;
    /* Locate */
    for (i = 0; i <= tree->bits; i++)
    {
        n = *cur;
        if (!n) return;  /* Not in the tree in the first place */
        stack[i] = cur;
        if (key & bit) cur = &n->right;
        else cur = &n->left;
        bit >>= 1;
    }
    /* Delete */
    if (tree->owns_memory && n->value) free(n->value);
    for (i = tree->bits; i >= 0; i--)
    {
        n = *stack[i];
        if (!n->left && !n->right)
        {
            free(n);
            *stack[i] = NULL;
        }
    }
    /* Compact */
    if (!tree->root) tree->bits = 0;
    while (tree->root && tree->root->right == NULL)
    {
        n = tree->root;
        tree->root = tree->root->left;
        tree->bits--;
        free(n);
    }
}

void bugle_radix_tree_set(bugle_radix_tree *tree, bugle_radix_tree_type key,
                          void *value)
{
    if (value) radix_tree_set(tree, key, value);
    else radix_tree_unset(tree, key);
}

void *bugle_radix_tree_get(const bugle_radix_tree *tree, bugle_radix_tree_type key)
{
    bugle_radix_node *n;
    bugle_radix_tree_type bit;
    int i;

    if (key > tree_max(tree->bits)) return NULL; /* Beyond range of tree */
    bit = tree_bit(tree->bits);
    n = tree->root;
    for (i = 0; i < tree->bits; i++)
    {
        if (!n) return NULL;
        if (key & bit) n = n->right;
        else n = n->left;
        bit >>= 1;
    }
    return n->value;
}

static void clear_recurse(bugle_radix_node *node,
                          bugle_radix_tree_type bit,
                          bool owns_memory)
{
    if (bit == 0)
    {
        if (owns_memory) free(node->value);
    }
    else
    {
        if (node->left) clear_recurse(node->left, bit >> 1, owns_memory);
        if (node->right) clear_recurse(node->right, bit >> 1, owns_memory);
    }
    free(node);
}

void bugle_radix_tree_clear(bugle_radix_tree *tree)
{
    if (tree->root) clear_recurse(tree->root,
                                  tree_bit(tree->bits),
                                  tree->owns_memory);
    tree->root = NULL;
    tree->bits = 0;
}

static void walk_recurse(const bugle_radix_node *node,
                         bugle_radix_tree_type value,
                         bugle_radix_tree_type bit,
                         void (*walker)(bugle_radix_tree_type, void *, void *),
                         void *data)
{
    if (bit == 0) (*walker)(value, node->value, data);
    else
    {
        if (node->left) walk_recurse(node->left, value, bit >> 1, walker, data);
        if (node->right) walk_recurse(node->right, value | bit, bit >> 1, walker, data);
    }
}

void bugle_radix_tree_walk(const bugle_radix_tree *tree,
                           void (*walker)(bugle_radix_tree_type, void *, void *),
                           void *data)
{
    if (tree->root) walk_recurse(tree->root,
                                 (bugle_radix_tree_type) 0,
                                 tree_bit(tree->bits),
                                 walker, data);
}
