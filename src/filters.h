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

#ifndef BUGLE_SRC_FILTERS_H
#define BUGLE_SRC_FILTERS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include "src/utils.h"
#include "common/linkedlist.h"
#include "common/bool.h"

struct filter_set_s;

typedef struct
{
    void *call_data;
} callback_data;

typedef bool (*filter_set_initialiser)(struct filter_set_s *);
typedef void (*filter_set_destructor)(struct filter_set_s *);
typedef bool (*filter_callback)(function_call *call, const callback_data *data);

typedef enum
{
    FILTER_SET_VARIABLE_BOOL,
    FILTER_SET_VARIABLE_INT,
    FILTER_SET_VARIABLE_UINT,
    FILTER_SET_VARIABLE_POSITIVE_INT,
    FILTER_SET_VARIABLE_STRING,
} filter_set_variable_type;

typedef struct filter_set_variable_info_s
{
    const char *name;
    filter_set_variable_type type;
    /* int, uint, posint: a long * (uint just enforces non-negative)
     * bool: a bool *
     * string: pointer to char *, which will then own the memory.
     *         If the old value was non-NULL, it is freed.
     * value may also be NULL, in which case a callback is needed.
     */
    void *value;
    /* If not NULL, then is called before value is assigned. It may
     * return false to abort the assignment. The parameter `value'
     * points to the value that will be assigned to the struct `value'
     * if the callback returns true.
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
    filter_set_initialiser init;
    filter_set_destructor done;
    const filter_set_variable_info *variables;
    ptrdiff_t call_state_offset;
    void *dl_handle;

    bool initialised;
    bool enabled;
} filter_set;

typedef struct
{
    const char *name;
    filter_set_initialiser init;
    filter_set_destructor done;
    const filter_set_variable_info *variables; /* NULL-terminated array */
    size_t call_state_space;
    const char *help;
} filter_set_info;

/* Functions to be used by the interceptor */

void initialise_filters(void);
bool filter_set_variable(filter_set *handle, const char *name, const char *text);
void bugle_enable_filter_set(filter_set *handle);
void bugle_disable_filter_set(filter_set *handle);
void run_filters(function_call *call);
void bugle_filters_help(void);

/* Functions to be used by the filter libraries, and perhaps the interceptor */

filter_set *bugle_register_filter_set(const filter_set_info *info);
filter *bugle_register_filter(filter_set *handle, const char *name);
void bugle_register_filter_catches(filter *handle, budgie_function f, filter_callback callback);
void bugle_register_filter_catches_all(filter *handle, filter_callback callback);
void bugle_register_filter_set_depends(const char *base, const char *dep);
void bugle_register_filter_depends(const char *after, const char *before);
void *bugle_get_filter_set_call_state(function_call *call, filter_set *handle);
filter_set *bugle_get_filter_set_handle(const char *name);
bool bugle_filter_set_is_enabled(const filter_set *handle);
void *bugle_get_filter_set_symbol(filter_set *handle, const char *name);

#endif /* BUGLE_SRC_FILTERS_H */
