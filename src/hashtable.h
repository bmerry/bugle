#ifndef BUGLE_SRC_HASHTABLE_H
#define BUGLE_SRC_HASHTABLE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "common/bool.h"

typedef struct
{
    char *key;
    const void *value;
} hash_entry;

typedef struct
{
    hash_entry *entries;
    size_t size;
    size_t count;
    int size_index;
} hash_table;


void initialise_hashing(void);

void hash_init(hash_table *table);
void hash_set(hash_table *table, const char *key, const void *value);
void *hash_get(const hash_table *table, const char *key);
void hash_clear(hash_table *table, bool free_data);

#endif /* !BUGLE_SRC_HASHTABLE_H */
