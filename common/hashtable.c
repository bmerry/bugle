/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007, 2009  Bruce Merry
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
#include <stddef.h>
#include <assert.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include <bugle/hashtable.h>
#include "common/threads.h"

/* Primes are used for hash table sizes */
static size_t primes[sizeof(size_t) * 8];

static bugle_bool is_prime(int x)
{
    int i;

    for (i = 2; i * i <= x; i++)
        if (x % i == 0) return BUGLE_FALSE;
    return BUGLE_TRUE;
}

bugle_thread_once_define(static, hash_once)

static void hash_initialise(void)
{
    int i;

    primes[0] = 0;
    primes[1] = 5;
    i = 1;
    while (primes[i] < ((size_t) -1) / 2)
    {
        primes[i + 1] = primes[i] * 2 + 1;
        i++;
        while (!is_prime(primes[i])) primes[i] += 2;
    }
    primes[i + 1] = (size_t) -1;
}

static inline size_t hash(const char *str)
{
    size_t h = 0;
    const char *ch;

    for (ch = str; *ch; ch++)
        h = (h + *ch) * 29;
    return h;
}

void bugle_hash_init(hash_table *table, void (*destructor)(void *))
{
    table->size = table->count = 0;
    table->size_index = 0;
    table->entries = 0;
    table->destructor = destructor;
}

/* Does not
 * - resize the table
 * - update the count
 * - copy the memory for the key
 * - check for duplicate keys
 * It should only be used for rehashing
 */
static void hash_set_fast(hash_table *table, char *key, void *value)
{
    size_t h;

    h = hash(key) % table->size;
    while (table->entries[h].key)
        if (++h == table->size) h = 0;
    table->entries[h].key = key;
    table->entries[h].value = value;
}

void bugle_hash_set(hash_table *table, const char *key, void *value)
{
    hash_table big;
    size_t i;
    size_t h;

    if (table->count >= table->size / 2
        && table->size < (size_t) -1)
    {
        bugle_thread_once(hash_once, hash_initialise);
        big.size_index = table->size_index + 1;
        big.size = primes[big.size_index];
        big.entries = BUGLE_CALLOC(big.size, hash_table_entry);
        big.count = 0;
        big.destructor = table->destructor;
        for (i = 0; i < table->size; i++)
            if (table->entries[i].key)
                hash_set_fast(&big, table->entries[i].key,
                                    table->entries[i].value);
        if (table->entries) free(table->entries);
        *table = big;
    }

    h = hash(key) % table->size;
    while (table->entries[h].key && strcmp(key, table->entries[h].key) != 0)
        if (++h == table->size) h = 0;
    if (!table->entries[h].key)
    {
        table->entries[h].key = bugle_strdup(key);
        table->count++;
    }
    else if (table->destructor)
        table->destructor(table->entries[h].value);
    table->entries[h].value = value;
}

bugle_bool bugle_hash_count(const hash_table *table, const char *key)
{
    size_t h;

    if (!table->entries) return BUGLE_FALSE;
    h = hash(key) % table->size;
    while (table->entries[h].key && strcmp(key, table->entries[h].key) != 0)
        if (++h == table->size) h = 0;
    return table->entries[h].key != NULL;
}

void *bugle_hash_get(const hash_table *table, const char *key)
{
    size_t h;

    if (!table->entries) return NULL;
    h = hash(key) % table->size;
    while (table->entries[h].key && strcmp(key, table->entries[h].key) != 0)
        if (++h == table->size) h = 0;
    if (table->entries[h].key) return table->entries[h].value;
    else return NULL;
}

void bugle_hash_clear(hash_table *table)
{
    size_t i;

    if (table->entries)
    {
        for (i = 0; i < table->size; i++)
            if (table->entries[i].key)
            {
                free(table->entries[i].key);
                if (table->destructor)
                    table->destructor(table->entries[i].value);
            }
        free(table->entries);
    }
    table->entries = NULL;
    table->size = table->count = 0;
    table->size_index = 0;
}

const hash_table_entry *bugle_hash_next(hash_table *table, const hash_table_entry *e)
{
    const hash_table_entry *end;

    e++;
    end = table->entries + table->size;
    while (e < end && !e->key) e++;
    if (e == end) return NULL;
    else return e;
}

const hash_table_entry *bugle_hash_begin(hash_table *table)
{
    if (!table->entries) return NULL;
    else if (table->entries->key) return table->entries;
    else return bugle_hash_next(table, table->entries);
}

/* void * based hashing */
/* FIXME: a lot of this code should be unified with the regular hash code;
 * either using actually shared code, or hacks with #includes or inlines
 * to maintain efficiency but still cut down maintainence
 */

static inline size_t hashptr(const void *str)
{
    return (const char *) str - (const char *) NULL;
}

void bugle_hashptr_init(hashptr_table *table, void (*destructor)(void *))
{
    table->size = table->count = 0;
    table->size_index = 0;
    table->entries = 0;
    table->destructor = destructor;
}

/* Does not
 * - resize the table
 * - update the count
 * - copy the memory for the key
 * - check for duplicate keys
 * It should only be used for rehashing
 */
static void hashptr_set_fast(hashptr_table *table, const void *key, void *value)
{
    size_t h;

    h = hashptr(key) % table->size;
    while (table->entries[h].key)
        if (++h == table->size) h = 0;
    table->entries[h].key = key;
    table->entries[h].value = value;
}

void bugle_hashptr_set(hashptr_table *table, const void *key, void *value)
{
    hashptr_table big;
    size_t i;
    size_t h;

    if (table->count >= table->size / 2
        && table->size < (size_t) -1)
    {
        bugle_thread_once(hash_once, hash_initialise);
        big.size_index = table->size_index + 1;
        big.size = primes[big.size_index];
        big.entries = BUGLE_CALLOC(big.size, hashptr_table_entry);
        big.count = 0;
        big.destructor = table->destructor;
        for (i = 0; i < table->size; i++)
            if (table->entries[i].key)
                hashptr_set_fast(&big, table->entries[i].key,
                                 table->entries[i].value);
        if (table->entries) free(table->entries);
        *table = big;
    }

    h = hashptr(key) % table->size;
    while (table->entries[h].key && key != table->entries[h].key)
        if (++h == table->size) h = 0;
    if (!table->entries[h].key)
    {
        table->entries[h].key = key;
        table->count++;
    }
    else if (table->destructor)
        table->destructor(table->entries[h].value);
    table->entries[h].value = value;
}

bugle_bool bugle_hashptr_count(const hashptr_table *table, const void *key)
{
    size_t h;

    if (!table->entries) return BUGLE_FALSE;
    h = hashptr(key) % table->size;
    while (table->entries[h].key && table->entries[h].key != key)
        if (++h == table->size) h = 0;
    return table->entries[h].key != NULL;
}

void *bugle_hashptr_get(const hashptr_table *table, const void *key)
{
    size_t h;

    if (!table->entries) return NULL;
    h = hashptr(key) % table->size;
    while (table->entries[h].key && table->entries[h].key != key)
        if (++h == table->size) h = 0;
    if (table->entries[h].key) return table->entries[h].value;
    else return NULL;
}

void bugle_hashptr_clear(hashptr_table *table)
{
    size_t i;

    if (table->entries)
    {
        for (i = 0; i < table->size; i++)
            if (table->entries[i].key)
            {
                if (table->destructor)
                    table->destructor(table->entries[i].value);
            }
        free(table->entries);
    }
    table->entries = NULL;
    table->size = table->count = 0;
    table->size_index = 0;
}

const hashptr_table_entry *bugle_hashptr_next(hashptr_table *table, const hashptr_table_entry *e)
{
    const hashptr_table_entry *end;

    e++;
    end = table->entries + table->size;
    while (e < end && !e->key) e++;
    if (e == end) return NULL;
    else return e;
}

const hashptr_table_entry *bugle_hashptr_begin(hashptr_table *table)
{
    if (!table->entries) return NULL;
    else if (table->entries->key) return table->entries;
    else return bugle_hashptr_next(table, table->entries);
}
