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

#ifndef BUGLE_SRC_FILTERS_H
#define BUGLE_SRC_FILTERS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include <ltdl.h>
#include "src/utils.h"
#include "common/linkedlist.h"
#include "common/bool.h"

struct filter_set_s;
struct function_call_s;

typedef struct
{
    void *call_data;
    struct filter_set_s *filter_set_handle;
} callback_data;

typedef bool (*filter_set_loader)(struct filter_set_s *);
typedef void (*filter_set_unloader)(struct filter_set_s *);
typedef void (*filter_set_activator)(struct filter_set_s *);
typedef void (*filter_set_deactivator)(struct filter_set_s *);
typedef bool (*filter_callback)(struct function_call_s *call, const callback_data *data);

typedef enum
{
    FILTER_SET_VARIABLE_BOOL,
    FILTER_SET_VARIABLE_INT,
    FILTER_SET_VARIABLE_UINT,
    FILTER_SET_VARIABLE_POSITIVE_INT,
    FILTER_SET_VARIABLE_FLOAT,
    FILTER_SET_VARIABLE_STRING,
    FILTER_SET_VARIABLE_KEY,
    FILTER_SET_VARIABLE_CUSTOM
} filter_set_variable_type;

typedef struct filter_set_variable_info_s
{
    const char *name;
    const char *help;
    filter_set_variable_type type;
    /* int, uint, posint: a long * (uint just enforces non-negative)
     * float: a float *
     * bool: a bool *
     * string: pointer to char *, which will then own the memory.
     *         If the old value was non-NULL, it is freed.
     * key: a xevent_key *
     * custom: pointer to whatever you like, even NULL. The callback
     *         is required in this case.
     */
    void *value;
    /* If not NULL, then is called before value is assigned. It may
     * return false to abort the assignment. The parameter `value'
     * points to the value that will be assigned to the struct `value'
     * if the callback returns true (in the case of CUSTOM, no copy
     * is performed - you get the real pointer).
     */
    bool (*callback)(const struct filter_set_variable_info_s *var,
                     const char *text, const void *value);
} filter_set_variable_info;

typedef struct
{
    const char *name;
    struct filter_set_s *parent;
    /* List of filter_catcher structs. The list owns the struct memory */
    bugle_linked_list callbacks;
} filter;

typedef struct filter_set_s
{
    const char *name;
    const char *help;
    bugle_linked_list filters;
    filter_set_loader load;
    filter_set_unloader unload;
    filter_set_activator activate;
    filter_set_deactivator deactivate;
    const filter_set_variable_info *variables;
    ptrdiff_t call_state_offset;
    lt_dlhandle dl_handle;

    bool loaded;
    bool active;
} filter_set;

typedef struct
{
    const char *name;
    filter_set_loader load;
    filter_set_unloader unload;
    filter_set_activator activate;
    filter_set_deactivator deactivate;
    const filter_set_variable_info *variables; /* NULL-terminated array */
    size_t call_state_space;
    const char *help;
} filter_set_info;

/* Functions to be used by the interceptor */

void initialise_filters(void);
bool filter_set_variable(filter_set *handle, const char *name, const char *text);
void bugle_load_filter_set(filter_set *handle, bool activate);
void bugle_activate_filter_set(filter_set *handle);
void bugle_deactivate_filter_set(filter_set *handle);
void bugle_activate_filter_set_deferred(filter_set *handle);
void bugle_deactivate_filter_set_deferred(filter_set *handle);
void filter_compute_order(void); /* Called after all filtersets loaded to determine filter order */
void filter_set_bypass(void);    /* Called after all filtersets loaded to set values in budgie_bypass */
void run_filters(struct function_call_s *call);
void bugle_filters_help(void);

/* Functions to be used by the filter libraries, and perhaps the interceptor */

filter_set *bugle_register_filter_set(const filter_set_info *info);
filter *bugle_register_filter(filter_set *handle, const char *name);
void bugle_register_filter_catches(filter *handle, budgie_group g, bool inactive, filter_callback callback);
void bugle_register_filter_catches_all(filter *handle, bool inactive, filter_callback callback);
/* Like bugle_register_filter_catches, but on a per-function rather than
 * per-group basis.
 */
void bugle_register_filter_catches_function(filter *handle, budgie_function f, bool inactive, filter_callback callback);
void bugle_register_filter_set_depends(const char *base, const char *dep);
void bugle_register_filter_depends(const char *after, const char *before);
void *bugle_get_filter_set_call_state(struct function_call_s *call, filter_set *handle);
filter_set *bugle_get_filter_set_handle(const char *name);
bool bugle_filter_set_is_loaded(const filter_set *handle);
bool bugle_filter_set_is_active(const filter_set *handle);
void *bugle_get_filter_set_symbol(filter_set *handle, const char *name);

#endif /* BUGLE_SRC_FILTERS_H */
