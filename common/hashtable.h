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

#ifndef BUGLE_COMMON_HASHTABLE_H
#define BUGLE_COMMON_HASHTABLE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    char *key;
    void *value;
} hash_table_entry;

typedef struct
{
    hash_table_entry *entries;
    size_t size;
    size_t count;
    int size_index;
    void (*destructor)(void *);
} hash_table;

/* Destructor may be NULL, free, or something custom. It will be called
 * even if the value is NULL.
 */
void bugle_hash_init(hash_table *table, void (*destructor)(void *));
void bugle_hash_set(hash_table *table, const char *key, void *value);
/* Determines whether the key is present */
bool bugle_hash_count(const hash_table *table, const char *key);
/* Returns NULL if key absent OR if value is NULL */
void *bugle_hash_get(const hash_table *table, const char *key);
void bugle_hash_clear(hash_table *table);

/* Walk the hash table. A walker loop looks like this:
 * for (h = bugle_hash_begin(&table); h; h = bugle_hash_next(&table, h))
 */
const hash_table_entry *bugle_hash_begin(hash_table *table);
const hash_table_entry *bugle_hash_next(hash_table *table, const hash_table_entry *e);

/* A similar implementation but with void * instead of string keys.
 * The data in the void * is irrelevant and in fact the void * is
 * never dereferenced, so one can cast integers to void *.
 */
typedef struct
{
    const void *key;
    void *value;
} hashptr_table_entry;

typedef struct
{
    hashptr_table_entry *entries;
    size_t size;
    size_t count;
    int size_index;
    void (*destructor)(void *);
} hashptr_table;

void bugle_hashptr_init(hashptr_table *table, void (*destructor)(void *));
void bugle_hashptr_set(hashptr_table *table, const void *key, void *value);
bool bugle_hashptr_count(const hashptr_table *table, const void *key);
void *bugle_hashptr_get(const hashptr_table *table, const void *key);
void bugle_hashptr_clear(hashptr_table *table);

const hashptr_table_entry *bugle_hashptr_begin(hashptr_table *table);
const hashptr_table_entry *bugle_hashptr_next(hashptr_table *table, const hashptr_table_entry *e);

#endif /* !BUGLE_COMMON_HASHTABLE_H */
