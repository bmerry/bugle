/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2009  Bruce Merry
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

#include <stddef.h>
#include <ltdl.h>
#include <bugle/linkedlist.h>
#include <bugle/objects.h>
#include <bugle/export.h>
#include <budgie/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct filter_set_s;
struct function_call_s;

typedef struct
{
    object *call_object;
    struct filter_set_s *filter_set_handle;
} callback_data;

typedef bool (*filter_set_loader)(struct filter_set_s *);
typedef void (*filter_set_unloader)(struct filter_set_s *);
typedef void (*filter_set_activator)(struct filter_set_s *);
typedef void (*filter_set_deactivator)(struct filter_set_s *);
typedef bool (*filter_callback)(function_call *call, const callback_data *data);

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
     * key: a bugle_input_key *
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
    linked_list callbacks;
} filter;

typedef struct filter_set_s
{
    const char *name;
    const char *help;
    linked_list filters;
    filter_set_loader load;
    filter_set_unloader unload;
    filter_set_activator activate;
    filter_set_deactivator deactivate;
    const filter_set_variable_info *variables;
    lt_dlhandle dl_handle;

    bool added;         /* Is listed in the config file or is depended upon */
    bool loaded;        /* Initialisation has been called */
    bool active;        /* Is actively intercepting events */
} filter_set;

typedef struct
{
    const char *name;
    filter_set_loader load;
    filter_set_unloader unload;
    filter_set_activator activate;
    filter_set_deactivator deactivate;
    const filter_set_variable_info *variables; /* NULL-terminated array */
    const char *help;
} filter_set_info;

/* Functions to be used by the interceptor only */
void filters_initialise(void);
bool filter_set_variable(filter_set *handle, const char *name, const char *text);
void filter_set_add(filter_set *handle, bool activate);
void filter_set_activate(filter_set *handle);
void filter_set_deactivate(filter_set *handle);
void filters_finalise(void); /* Called after all filtersets added to do last initialisation */
void filters_run(function_call *call);
void filters_help(void);

/* Initialisation functions to be used by the filter libraries, and perhaps the interceptor */
BUGLE_EXPORT_PRE filter_set *bugle_filter_set_new(const filter_set_info *info) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void        bugle_filter_set_depends(const char *base, const char *dep) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void        bugle_filter_set_order(const char *before, const char *after) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE filter *    bugle_filter_new(filter_set *handle, const char *name) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void        bugle_filter_order(const char *before, const char *after) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void        bugle_filter_catches(filter *handle, const char *group, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
/* Like bugle_filter_catches, but per-function rather than per-group. */
BUGLE_EXPORT_PRE void        bugle_filter_catches_function(filter *handle, const char *f, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
/* Like bugle_filter_catch_function, but takes a budgie_function not a string */
BUGLE_EXPORT_PRE void        bugle_filter_catches_function_id(filter *handle, budgie_function, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void        bugle_filter_catches_all(filter *handle, bool inactive, filter_callback callback) BUGLE_EXPORT_POST;

extern BUGLE_EXPORT_PRE object_class *bugle_call_class BUGLE_EXPORT_POST;

/* Other run-time functions */
BUGLE_EXPORT_PRE filter_set *bugle_filter_set_get_handle(const char *name) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE bool        bugle_filter_set_is_loaded(const filter_set *handle) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE bool        bugle_filter_set_is_active(const filter_set *handle) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void *      bugle_filter_set_get_symbol(filter_set *handle, const char *name) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void        bugle_filter_set_activate_deferred(filter_set *handle) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void        bugle_filter_set_deactivate_deferred(filter_set *handle) BUGLE_EXPORT_POST;

/* Function exported by each filter - prototyped to ensure that it
 * has the correct symbol visibility.
 */
BUGLE_EXPORT_PRE void        bugle_initialise_filter_library(void) BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* BUGLE_SRC_FILTERS_H */
