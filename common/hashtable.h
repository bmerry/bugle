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

#ifndef BUGLE_COMMON_HASHTABLE_H
#define BUGLE_COMMON_HASHTABLE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "common/bool.h"

typedef struct
{
    char *key;
    void *value;
} bugle_hash_entry;

typedef struct
{
    bugle_hash_entry *entries;
    size_t size;
    size_t count;
    int size_index;
} bugle_hash_table;

void bugle_initialise_hashing(void);

void bugle_hash_init(bugle_hash_table *table);
void bugle_hash_set(bugle_hash_table *table, const char *key, void *value);
void *bugle_hash_get(const bugle_hash_table *table, const char *key);
void bugle_hash_clear(bugle_hash_table *table, bool free_data);

/* Walk the hash table. A walker loop looks like this:
 * for (h = bugle_hash_begin(&table); h; h = bugle_hash_next(&table, h))
 */
const bugle_hash_entry *bugle_hash_begin(bugle_hash_table *table);
const bugle_hash_entry *bugle_hash_next(bugle_hash_table *table, const bugle_hash_entry *e);

/* A similar implementation but with void * instead of string keys.
 * The data in the void * is irrelevant.
 */
typedef struct
{
    const void *key;
    void *value;
} bugle_hashptr_entry;

typedef struct
{
    bugle_hashptr_entry *entries;
    size_t size;
    size_t count;
    int size_index;
} bugle_hashptr_table;

void bugle_hashptr_init(bugle_hashptr_table *table);
void bugle_hashptr_set(bugle_hashptr_table *table, const void *key, void *value);
void *bugle_hashptr_get(const bugle_hashptr_table *table, const void *key);
void bugle_hashptr_clear(bugle_hashptr_table *table, bool free_data);

const bugle_hashptr_entry *bugle_hashptr_begin(bugle_hashptr_table *table);
const bugle_hashptr_entry *bugle_hashptr_next(bugle_hashptr_table *table, const bugle_hashptr_entry *e);

#endif /* !BUGLE_COMMON_HASHTABLE_H */
