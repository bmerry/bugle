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

#ifndef BUGLE_BUDGIELIB_STATE_H
#define BUGLE_BUDGIELIB_STATE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include "budgieutils.h"
#include "common/bool.h"

/* Every state specification has the following:
 * - a key type, and a comparator for that type (only for index-type nodes)
 * - static, named children
 * - up to one dynamic child (named [])
 * - information about the data stored directly in this state (optional)
 * - information about how to update the state (optional)
 * - a constructor (optional)
 *
 * Keys are used for fast access to children, when the child is dynamic
 * or is only known at run-time. Only dynamic children have keys.
 *
 * Each instance has the following:
 * - a key value
 * - a name, for human-readable addressing
 * - storage for the data (if any)
 * - an array of static children
 * - an array of dynamic children, sorted by key
 * - accessors to the static children
 */

struct state_generic_s;

typedef struct state_spec_s
{
    const char *name;
    const struct state_spec_s **children;
    const struct state_spec_s *indexed;

    budgie_type key_type; /* NULL_TYPE for no keys */
    int (*key_compare)(const void *a, const void *b); /* NULL for no ordering */

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
    void *key;
    char *name; /* pointer to spec->name if static, malloc'ed if dynamic */
    struct state_generic_s *parent;
    struct state_generic_s **children; /* named */
    struct state_generic_s **indexed;  /* malloced */
    int num_indexed;
    int max_indexed;
    void *data;
} state_generic;

struct state_root_s;

void budgie_update_state(state_generic *state);
void budgie_update_state_recursive(state_generic *state);
void budgie_initialise_state(state_generic *, state_generic *parent);
state_generic *budgie_create_state(const state_spec *spec, state_generic *parent);
void budgie_destroy_state(state_generic *state, bool indexed);
void *budgie_get_state_cached(state_generic *state);
void *budgie_get_state_current(state_generic *state);
struct state_root_s *budgie_get_root_state(void);

/* index-specific stuff */
state_generic *budgie_add_state_index(state_generic *node, const void *key, const char *name);
state_generic *budgie_get_state_index(state_generic *node, const void *key);
state_generic *budgie_get_state_index_number(state_generic *node, int number);
state_generic *budgie_get_state_by_name(state_generic *base, const char *name);
void budgie_remove_state_index(state_generic *node, const void *key);

#endif /* !BUGLE_BUDGIELIB_STATE_H */
