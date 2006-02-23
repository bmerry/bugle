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

/* Implements a radix tree; this is a type of binary tree that needs no
 * balancing but which can only store non-negative integers. Each split
 * defines one bit of the integer.
 *
 * The design is based around storing OpenGL object ids for the object
 * tracker (trackobjects.c). Since OpenGL implementations are likely to
 * use small integers, we make one optimisation: the tree stores only
 * as many bits as needed. When a larger integer comes along it is expanded.
 */

#ifndef BUGLE_COMMON_RADIXTREE_H
#define BUGLE_COMMON_RADIXTREE_H
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "common/bool.h"
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long bugle_radix_tree_type;

typedef struct bugle_radix_tree_t
{
    struct bugle_radix_node_t *root;
    bool owns_memory;
    int bits;
} bugle_radix_tree;

void bugle_radix_tree_init(bugle_radix_tree *tree, bool owns_memory);
void bugle_radix_tree_set(bugle_radix_tree *tree, bugle_radix_tree_type key, void *value);
void *bugle_radix_tree_get(const bugle_radix_tree *tree, bugle_radix_tree_type key);
void bugle_radix_tree_clear(bugle_radix_tree *tree);
void bugle_radix_tree_walk(const bugle_radix_tree *tree,
                           void (*walker)(bugle_radix_tree_type, void *, void *),
                           void *data);

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_COMMON_LINKEDLIST_H */
