#ifndef BUGLE_BUDGIELIB_STATE_H
#define BUGLE_BUDGIELIB_STATE_H

#include <stddef.h>
#include "budgieutils.h"

/*
 USE CASES
 - obtain access to a known child
 - obtain access to a named child
 - update state
 - get cached state
 - get current state
 - modify state
 - get state as string
 - enumerate all children
 - enumerate

 TYPES OF INDICES
 - string
 - sequential (0-n)
 - handle (object handles)
 - pointers (e.g. context)
 */

struct state_generic_s;

typedef struct state_spec_s
{
    const char *name;
    const struct state_spec_s **children;
    const struct state_spec_s *indexed;
    int (*key_compare)(const void *a, const void *b);
    budgie_type data_type;
    int data_length;
    size_t instance_size;
    void (*updater)(struct state_generic_s *state);
    void (*constructor)(struct state_generic_s *state,
                        struct state_generic_s *parent);
} state_spec;

typedef struct state_generic_s
{
    const state_spec *spec;
    const void *key;
    struct state_generic_s *parent;
    struct state_generic_s **children;
    struct state_generic_s **indexed; /* malloced */
    int num_indexed;
    int max_indexed;
    void *data;
} state_generic;

struct state_root_s;

void update_state(state_generic *state);
void update_state_recursive(state_generic *state);
void initialise_state(state_generic *, state_generic *parent);
state_generic *create_state(const state_spec *spec, state_generic *parent);
void destroy_state(state_generic *state);
void *get_state_cached(state_generic *state);
void *get_state_current(state_generic *state);
struct state_root_s *get_root_state(void);

/* index-specific stuff */
state_generic *add_state_index(state_generic *node, const void *key);
state_generic *get_state_index(state_generic *node, const void *key);
state_generic *get_state_index_number(state_generic *node, int number);
void remove_state_index(state_generic *node, const void *key);

#endif /* !BUGLE_BUDGIELIB_STATE_H */
