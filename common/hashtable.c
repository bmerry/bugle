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
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include "hashtable.h"
#include "common/safemem.h"
#include "common/bool.h"

/* Primes are used for hash table sizes */
static size_t primes[sizeof(size_t) * 8];
static bool primes_initialised = false;

static bool is_prime(int x)
{
    int i;

    for (i = 2; i * i <= x; i++)
        if (x % i == 0) return false;
    return true;
}

void bugle_initialise_hashing(void)
{
    int i;

    primes[0] = 0;
    primes[1] = 5;
    i = 1;
    while (primes[i] < ((size_t) -1) / 2)
    {
        primes[i + 1] = primes[i] * 2 + 1;
        i++;
        while (!is_prime(primes[i])) primes[i]++;
    }
    primes[i + 1] = (size_t) -1;
    primes_initialised = true;
}

static inline size_t hash(const char *str)
{
    size_t h = 0;
    const char *ch;

    for (ch = str; *ch; ch++)
        h = (h + *ch) * 29;
    return h;
}

void bugle_hash_init(bugle_hash_table *table, bool owns_memory)
{
    table->size = table->count = 0;
    table->size_index = 0;
    table->entries = 0;
    table->owns_memory = owns_memory;
}

/* Does not
 * - resize the table
 * - update the count
 * - copy the memory for the key
 * - check for duplicate keys
 * It should only be used for rehashing
 */
static void hash_set_fast(bugle_hash_table *table, char *key, void *value)
{
    size_t h;

    h = hash(key) % table->size;
    while (table->entries[h].key)
        if (++h == table->size) h = 0;
    table->entries[h].key = key;
    table->entries[h].value = value;
}

void bugle_hash_set(bugle_hash_table *table, const char *key, void *value)
{
    bugle_hash_table big;
    size_t i;
    size_t h;

    if (table->count >= table->size / 2
        && table->size < (size_t) -1)
    {
        assert(primes_initialised);
        big.size_index = table->size_index + 1;
        big.size = primes[big.size_index];
        big.entries = (bugle_hash_entry *) bugle_calloc(big.size, sizeof(bugle_hash_entry));
        big.count = 0;
        big.owns_memory = table->owns_memory;
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
    else if (table->owns_memory && table->entries[h].value)
        free(table->entries[h].value);
    table->entries[h].value = value;
}

void *bugle_hash_get(const bugle_hash_table *table, const char *key)
{
    size_t h;

    if (!table->entries) return NULL;
    h = hash(key) % table->size;
    while (table->entries[h].key && strcmp(key, table->entries[h].key) != 0)
        if (++h == table->size) h = 0;
    if (table->entries[h].key)
    {
        if (table->entries[h].value) return table->entries[h].value;
        return (void *) table->entries[h].key;
    }
    else
        return NULL;
}

void bugle_hash_clear(bugle_hash_table *table)
{
    size_t i;

    if (table->entries)
    {
        for (i = 0; i < table->size; i++)
            if (table->entries[i].key)
            {
                free(table->entries[i].key);
                if (table->owns_memory && table->entries[i].value)
                    free(table->entries[i].value);
            }
        free(table->entries);
    }
    table->entries = NULL;
    table->size = table->count = 0;
    table->size_index = 0;
}

const bugle_hash_entry *bugle_hash_next(bugle_hash_table *table, const bugle_hash_entry *e)
{
    const bugle_hash_entry *end;

    e++;
    end = table->entries + table->size;
    while (e < end && !e->key) e++;
    if (e == end) return NULL;
    else return e;
}

const bugle_hash_entry *bugle_hash_begin(bugle_hash_table *table)
{
    if (!table->entries) return NULL;
    else if (table->entries->key) return table->entries;
    else return bugle_hash_next(table, table->entries);
}

/* void * based hashing */

static inline size_t hashptr(const void *str)
{
    return (const char *) str - (const char *) NULL;
}

void bugle_hashptr_init(bugle_hashptr_table *table, bool owns_memory)
{
    table->size = table->count = 0;
    table->size_index = 0;
    table->entries = 0;
    table->owns_memory = owns_memory;
}

/* Does not
 * - resize the table
 * - update the count
 * - copy the memory for the key
 * - check for duplicate keys
 * It should only be used for rehashing
 */
static void hashptr_set_fast(bugle_hashptr_table *table, const void *key, void *value)
{
    size_t h;

    h = hashptr(key) % table->size;
    while (table->entries[h].key)
        if (++h == table->size) h = 0;
    table->entries[h].key = key;
    table->entries[h].value = value;
}

void bugle_hashptr_set(bugle_hashptr_table *table, const void *key, void *value)
{
    bugle_hashptr_table big;
    size_t i;
    size_t h;

    if (table->count >= table->size / 2
        && table->size < (size_t) -1)
    {
        assert(primes_initialised);
        big.size_index = table->size_index + 1;
        big.size = primes[big.size_index];
        big.entries = (bugle_hashptr_entry *) bugle_calloc(big.size, sizeof(bugle_hashptr_entry));
        big.count = 0;
        big.owns_memory = table->owns_memory;
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
    else if (table->owns_memory && table->entries[h].value)
        free(table->entries[h].value);
    table->entries[h].value = value;
}

void *bugle_hashptr_get(const bugle_hashptr_table *table, const void *key)
{
    size_t h;

    if (!table->entries) return NULL;
    h = hashptr(key) % table->size;
    while (table->entries[h].key != key)
        if (++h == table->size) h = 0;
    if (table->entries[h].key)
    {
        if (table->entries[h].value) return table->entries[h].value;
        return (void *) key;
    }
    else
        return NULL;
}

void bugle_hashptr_clear(bugle_hashptr_table *table)
{
    size_t i;

    if (table->entries)
    {
        for (i = 0; i < table->size; i++)
            if (table->entries[i].key)
            {
                if (table->owns_memory && table->entries[i].value)
                    free(table->entries[i].value);
            }
        free(table->entries);
    }
    table->entries = NULL;
    table->size = table->count = 0;
    table->size_index = 0;
}

const bugle_hashptr_entry *bugle_hashptr_next(bugle_hashptr_table *table, const bugle_hashptr_entry *e)
{
    const bugle_hashptr_entry *end;

    e++;
    end = table->entries + table->size;
    while (e < end && !e->key) e++;
    if (e == end) return NULL;
    else return e;
}

const bugle_hashptr_entry *bugle_hashptr_begin(bugle_hashptr_table *table)
{
    if (!table->entries) return NULL;
    else if (table->entries->key) return table->entries;
    else return bugle_hashptr_next(table, table->entries);
}
