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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _POSIX_SOURCE /* For flockfile */
#define _BSD_SOURCE /* For finite() */
#define _XOPEN_SOURCE 600 /* For strtof */
#include "src/utils.h"
#include "src/lib.h"
#include "src/glfuncs.h"
#include "src/xevent.h"
#include "src/filters.h"
#include "src/log.h"
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include "common/threads.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

typedef struct
{
    filter *parent;
    budgie_function function;
    bool inactive;    /* True if callback should be called even when filterset is inactive */
    filter_callback callback;
} filter_catcher;

static bugle_linked_list filter_sets;

/* As the filter-sets are loaded, loaded_filters is populated with
 * the loaded filters in arbitrary order. Once all filter-sets are
 * loaded, the initialisation code calls filter_compute_order
 * to do a topological sort on the filters in loaded_filters. No
 * locking is required because this all happens in the initialisation
 * code.
 *
 * The active_callbacks list contains the list of callback in
 * active filters. Each active_callback list entry points to a
 * filter_catcher in the original filter structure. It is updated lazily,
 * based on the value of active_dirty. Both these variables are
 * protected by active_callbacks_mutex (as are the ->active flags on
 * filter-sets). If there are both activations
 * and deactivations within the same call, the order is not guaranted
 * to be preserved (this may eventually be fixed). Similarly, the
 * result of bugle_filter_set_is_active will reflect the old values
 * until the end of the call.
 *
 * If a filter wishes to activate or deactivative a filter-set (its own
 * or another), it should call bugle_activate_filter_set_deferred
 * or bugle_deactivate_filter_set_deferred. This causes the change to
 * happen only after the current call has completed processing (which
 * removes the need to recompute the active filters while processing
 * them), and also prevents a self-deadlock.
 */
static bugle_linked_list loaded_filters;

static bugle_linked_list active_callbacks[NUMBER_OF_FUNCTIONS];
static bool active_dirty = false;
static bugle_linked_list activations_deferred;
static bugle_linked_list deactivations_deferred;
static bugle_thread_mutex_t active_callbacks_mutex = BUGLE_THREAD_MUTEX_INITIALIZER;

/* hash table of linked lists of strings */
static bugle_hash_table filter_dependencies;
static bugle_linked_list filter_set_dependencies[2];
static void *call_data = NULL;
static size_t call_data_size = 0; /* FIXME: turn into an object */

static lt_dlhandle current_dl_handle = NULL;

static void bugle_deactivate_filter_set_nolock(filter_set *handle);

static void destroy_filter_set_r(filter_set *handle)
{
    bugle_list_node *i, *j;
    filter *f;
    filter_set *s;

    if (handle->loaded)
    {
        handle->loaded = false;
        /* deps */
        for (i = bugle_list_head(&filter_set_dependencies[0]),
             j = bugle_list_head(&filter_set_dependencies[1]);
             i;
             i = bugle_list_next(i), j = bugle_list_next(j))
        {
            if (strcmp(handle->name, (const char *) bugle_list_data(j)) == 0)
            {
                s = bugle_get_filter_set_handle((const char *) bugle_list_data(i));
                destroy_filter_set_r(s);
            }
        }

        bugle_deactivate_filter_set_nolock(handle);
        if (handle->unload) (*handle->unload)(handle);

        for (j = bugle_list_head(&handle->filters); j; j = bugle_list_next(j))
        {
            f = (filter *) bugle_list_data(j);
            bugle_list_clear(&f->callbacks);
            free(f);
        }
        bugle_list_clear(&handle->filters);
    }
}

static void destroy_filters(void *dummy)
{
    bugle_list_node *i;
    const bugle_hash_entry *j;
    filter_set *s;
    budgie_function k;
    bugle_linked_list *dep;

    bugle_list_clear(&loaded_filters);
    for (k = 0; k < NUMBER_OF_FUNCTIONS; k++)
        bugle_list_clear(&active_callbacks[k]);

    for (i = bugle_list_head(&filter_sets); i; i = bugle_list_next(i))
    {
        s = (filter_set *) bugle_list_data(i);
        if (s->loaded)
            destroy_filter_set_r(s);
    }
    for (i = bugle_list_head(&filter_sets); i; i = bugle_list_next(i))
    {
        s = (filter_set *) bugle_list_data(i);
        free(s);
    }
    for (j = bugle_hash_begin(&filter_dependencies); j; j = bugle_hash_next(&filter_dependencies, j))
        if (j->value)
        {
            dep = (bugle_linked_list *) j->value;
            bugle_list_clear(dep);
            free(dep);
        }

    bugle_list_clear(&filter_sets);
    bugle_hash_clear(&filter_dependencies);
    bugle_list_clear(&filter_set_dependencies[0]);
    bugle_list_clear(&filter_set_dependencies[1]);

    lt_dlexit();
}

static int initialise_filter_library(const char *filename, lt_ptr data)
{
    lt_dlhandle handle;
    void (*init)(void);

    handle = lt_dlopenext(filename);
    if (handle == NULL) return 0;

    init = (void (*)(void)) lt_dlsym(handle, "bugle_initialise_filter_library");
    if (init == NULL)
    {
        bugle_log_printf("filters", "initialise", BUGLE_LOG_WARNING,
                         "library %s did not export initialisation symbol",
                         filename);
        return 0;
    }
    current_dl_handle = handle;
    (*init)();
    current_dl_handle = NULL;
    return 0;
}

/* Note: initialise_filters is called after initialise_real, which sets
 * up ltdl. Hence we don't call lt_dlinit.
 */
void initialise_filters(void)
{
    DIR *dir;
    const char *libdir;
    budgie_function f;

    bugle_list_init(&filter_sets, false);
    bugle_list_init(&loaded_filters, false);
    for (f = 0; f < NUMBER_OF_FUNCTIONS; f++)
        bugle_list_init(&active_callbacks[f], false);
    bugle_list_init(&activations_deferred, false);
    bugle_list_init(&deactivations_deferred, false);
    bugle_hash_init(&filter_dependencies, false);
    bugle_list_init(&filter_set_dependencies[0], true);
    bugle_list_init(&filter_set_dependencies[1], true);

    libdir = getenv("BUGLE_FILTER_DIR");
    if (!libdir) libdir = PKGLIBDIR;
    dir = opendir(libdir);
    if (!dir)
    {
        bugle_log_printf("filters", "initialise", BUGLE_LOG_ERROR,
                         "failed to open %s: %s", libdir, strerror(errno));
        exit(1);
    }
    closedir(dir);

    lt_dlforeachfile(libdir, initialise_filter_library, NULL);

    bugle_atexit(destroy_filters, NULL);
}

bool filter_set_variable(filter_set *handle, const char *name, const char *value)
{
    const filter_set_variable_info *v;
    bool bool_value;
    long int_value;
    float float_value;
    char *string_value;
    char *end;
    xevent_key key_value;
    void *value_ptr = NULL;
    bool finite_value;

    for (v = handle->variables; v && v->name; v++)
    {
        if (strcmp(name, v->name) == 0)
        {
            switch (v->type)
            {
            case FILTER_SET_VARIABLE_BOOL:
                if (strcmp(value, "1") == 0
                    || strcmp(value, "yes") == 0
                    || strcmp(value, "true") == 0)
                    bool_value = true;
                else if (strcmp(value, "0") == 0
                         || strcmp(value, "no") == 0
                         || strcmp(value, "false") == 0)
                    bool_value = false;
                else
                {
                    bugle_log_printf(handle->name, "initialise", BUGLE_LOG_ERROR,
                                     "Expected 1|0|yes|no|true|false for %s in filter-set %s",
                                     name, handle->name);
                    return false;
                }
                value_ptr = &bool_value;
                break;
            case FILTER_SET_VARIABLE_INT:
            case FILTER_SET_VARIABLE_UINT:
            case FILTER_SET_VARIABLE_POSITIVE_INT:
                errno = 0;
                int_value = strtol(value, &end, 0);
                if (errno || !*value || *end)
                {
                    bugle_log_printf(handle->name, "initialise", BUGLE_LOG_ERROR,
                                     "Expected an integer for %s in filter-set %s",
                                     name, handle->name);
                    return false;
                }
                if (v->type == FILTER_SET_VARIABLE_UINT && int_value < 0)
                {
                    bugle_log_printf(handle->name, "initialise", BUGLE_LOG_ERROR,
                                     "Expected a non-negative integer for %s in filter-set %s",
                                     name, handle->name);
                    return false;
                }
                else if (v->type == FILTER_SET_VARIABLE_POSITIVE_INT && int_value <= 0)
                {
                    bugle_log_printf(handle->name, "initialise", BUGLE_LOG_ERROR,
                                     "Expected a positive integer for %s in filter-set %s",
                                     name, handle->name);
                    return false;
                }
                value_ptr = &int_value;
                break;
            case FILTER_SET_VARIABLE_FLOAT:
                errno = 0;
#if HAVE_STRTOF
                float_value = strtof(value, &end);
#else
                float_value = strtod(value, &end);
#endif
                if (errno || !*value || *end)
                {
                    bugle_log_printf(handle->name, "initialise", BUGLE_LOG_ERROR,
                                     "Expected a real number for %s in filter-set %s",
                                     name, handle->name);
                    return false;
                }

#if HAVE_ISFINITE
                finite_value = isfinite(float_value);
#elif HAVE_FINITE
                finite_value = finite(float_value);
#else
                finite_value = true;
#endif
                if (!finite_value)
                {
                    bugle_log_printf(handle->name, "initialise", BUGLE_LOG_ERROR,
                                     "Expected a finite real number for %s in filter-set %s",
                                     name, handle->name);
                    return false;
                }
                value_ptr = &float_value;
                break;
            case FILTER_SET_VARIABLE_STRING:
                string_value = bugle_strdup(value);
                value_ptr = &string_value;
                break;
            case FILTER_SET_VARIABLE_KEY:
                if (!bugle_xevent_key_lookup(value, &key_value))
                {
                    bugle_log_printf(handle->name, "initialise", BUGLE_LOG_ERROR,
                                     "Unknown key %s for %s in filter-set %s", value, name, handle->name);
                    return false;
                }
                value_ptr = &key_value;
                break;
            case FILTER_SET_VARIABLE_CUSTOM:
                value_ptr = v->value;
                break;
            }
            if (v->callback && !(*v->callback)(v, value, value_ptr))
            {
                if (v->type == FILTER_SET_VARIABLE_STRING)
                    free(string_value);
                return false;
            }
            else
            {
                if (v->value)
                {
                    switch (v->type)
                    {
                    case FILTER_SET_VARIABLE_BOOL:
                        *(bool *) v->value = bool_value;
                        break;
                    case FILTER_SET_VARIABLE_INT:
                    case FILTER_SET_VARIABLE_UINT:
                    case FILTER_SET_VARIABLE_POSITIVE_INT:
                        *(long *) v->value = int_value;
                        break;
                    case FILTER_SET_VARIABLE_FLOAT:
                        *(float *) v->value = float_value;
                        break;
                    case FILTER_SET_VARIABLE_STRING:
                        if (*(char **) v->value)
                            free(*(char **) v->value);
                        *(char **) v->value = string_value;
                        break;
                    case FILTER_SET_VARIABLE_KEY:
                        *(xevent_key *) v->value = key_value;
                        break;
                    case FILTER_SET_VARIABLE_CUSTOM:
                        break;
                    }
                }
                return true;
            }
        }
    }
    bugle_log_printf(handle->name, "initialise", BUGLE_LOG_ERROR,
                     "Unknown variable %s in filter-set %s",
                     name, handle->name);
    return false;
}

/* FIXME: deps should also be activated */
static void bugle_activate_filter_set_nolock(filter_set *handle)
{
    assert(handle);
    if (!handle->active)
    {
        if (handle->activate) (*handle->activate)(handle);
        handle->active = true;
        active_dirty = true;
    }
}

/* FIXME: reverse deps should also be deactivated */
static void bugle_deactivate_filter_set_nolock(filter_set *handle)
{
    assert(handle);
    if (handle->active)
    {
        if (handle->deactivate) (*handle->deactivate)(handle);
        handle->active = false;
        active_dirty = true;
    }
}

static void load_filter_set_r(filter_set *handle, bool activate)
{
    bugle_list_node *i, *j;
    filter_set *s;
    filter *f;

    if (!handle->loaded)
    {
        /* deps */
        for (i = bugle_list_head(&filter_set_dependencies[0]),
             j = bugle_list_head(&filter_set_dependencies[1]);
             i;
             i = bugle_list_next(i), j = bugle_list_next(j))
        {
            if (strcmp(handle->name, (const char *) bugle_list_data(i)) == 0)
            {
                s = bugle_get_filter_set_handle((const char *) bugle_list_data(j));
                if (!s)
                {
                    bugle_log_printf("filters", "load", BUGLE_LOG_ERROR,
                                     "filter-set %s depends on unknown filter-set %s",
                                     ((const char *) bugle_list_data(i)),
                                     ((const char *) bugle_list_data(j)));
                }
                load_filter_set_r(s, activate);
            }
        }
        /* Initialisation */
        if (!(*handle->load)(handle))
        {
            bugle_log_printf(handle->name, "load", BUGLE_LOG_ERROR,
                             "failed to initialise filter-set %s", handle->name);
            exit(1);
        }
        handle->loaded = true;

        for (i = bugle_list_head(&handle->filters); i; i = bugle_list_next(i))
        {
            f = (filter *) bugle_list_data(i);
            bugle_list_append(&loaded_filters, f);
        }

        if (activate)
        {
            bugle_activate_filter_set_nolock(handle);
            active_dirty = true;
        }
    }
}

void bugle_load_filter_set(filter_set *handle, bool activate)
{
    load_filter_set_r(handle, activate);
}

void bugle_activate_filter_set(filter_set *handle)
{
    bugle_thread_mutex_lock(&active_callbacks_mutex);
    bugle_activate_filter_set_nolock(handle);
    bugle_thread_mutex_unlock(&active_callbacks_mutex);
}

void bugle_deactivate_filter_set(filter_set *handle)
{
    bugle_thread_mutex_lock(&active_callbacks_mutex);
    bugle_deactivate_filter_set_nolock(handle);
    bugle_thread_mutex_unlock(&active_callbacks_mutex);
}

/* Note: these should be called only from within a callback, in which
 * case the lock is already held.
 */
void bugle_activate_filter_set_deferred(filter_set *handle)
{
    bugle_list_append(&activations_deferred, handle);
}

void bugle_deactivate_filter_set_deferred(filter_set *handle)
{
    bugle_list_append(&deactivations_deferred, handle);
}

typedef struct
{
    filter *f;
    int valence;
} order_data;

void filter_compute_order(void)
{
    bugle_linked_list ordered;
    bugle_linked_list queue;
    bugle_linked_list *deps;
    bugle_hash_table byname;  /* table of order_data structs for each filter by name */
    const char *name;
    int count = 0;            /* checks that everything made it without cycles */
    bugle_list_node *i, *j;
    filter *cur;
    order_data *info;

    bugle_list_init(&ordered, false);
    bugle_hash_init(&byname, true);
    for (i = bugle_list_head(&loaded_filters); i; i = bugle_list_next(i))
    {
        count++;
        info = (order_data *) bugle_malloc(sizeof(order_data));
        info->f = (filter *) bugle_list_data(i);
        info->valence = 0;
        bugle_hash_set(&byname, info->f->name, info);
    }

    /* fill in valences */
    for (i = bugle_list_head(&loaded_filters); i; i = bugle_list_next(i))
    {
        cur = (filter *) bugle_list_data(i);
        deps = (bugle_linked_list *) bugle_hash_get(&filter_dependencies, cur->name);
        if (deps)
        {
            for (j = bugle_list_head(deps); j; j = bugle_list_next(j))
            {
                name = (const char *) bugle_list_data(j);
                info = (order_data *) bugle_hash_get(&byname, name);
                if (info) /* otherwise a non-loaded filter */
                    info->valence++;
            }
        }
    }

    /* prime the queue */
    bugle_list_init(&queue, false);
    for (i = bugle_list_head(&loaded_filters); i; i = bugle_list_next(i))
    {
        cur = (filter *) bugle_list_data(i);
        info = (order_data *) bugle_hash_get(&byname, cur->name);
        if (info->valence == 0)
            bugle_list_append(&queue, cur);
    }

    /* do a topological walk, starting at the back */
    while (bugle_list_head(&queue))
    {
        count--;
        cur = (filter *) bugle_list_data(bugle_list_head(&queue));
        bugle_list_erase(&queue, bugle_list_head(&queue));
        deps = (bugle_linked_list *) bugle_hash_get(&filter_dependencies, cur->name);
        if (deps)
        {
            for (j = bugle_list_head(deps); j; j = bugle_list_next(j))
            {
                name = (const char *) bugle_list_data(j);
                info = (order_data *) bugle_hash_get(&byname, name);
                if (info) /* otherwise not a loaded filter */
                {
                    info->valence--;
                    if (info->valence == 0)
                        bugle_list_append(&queue, info->f);
                }
            }
        }
        bugle_list_prepend(&ordered, cur);
    }

    if (count > 0)
    {
        bugle_log("filters", "load", BUGLE_LOG_ERROR,
                  "cyclic dependency between filters");
        exit(1);
    }
    /* clean up and replace old version */
    bugle_list_clear(&queue);
    bugle_hash_clear(&byname);
    bugle_list_clear(&loaded_filters);
    loaded_filters = ordered;
}

void filter_set_bypass(void)
{
    bugle_list_node *i, *j;
    bool bypass[NUMBER_OF_FUNCTIONS];
    filter *cur;
    filter_catcher *catcher;

    memset(bypass, 1, sizeof(bypass));
    for (i = bugle_list_head(&loaded_filters); i; i = bugle_list_next(i))
    {
        cur = (filter *) bugle_list_data(i);
        for (j = bugle_list_tail(&cur->callbacks); j; j = bugle_list_prev(j))
        {
            catcher = (filter_catcher *) bugle_list_data(j);
            if (strcmp(catcher->parent->name, "invoke") != 0)
                bypass[catcher->function] = false;
        }
    }
    memcpy(budgie_bypass, bypass, sizeof(budgie_bypass));
}

/* Note: caller must take mutexes */
static void compute_active_callbacks(void)
{
    bugle_list_node *i, *j;
    filter *cur;
    budgie_function func;
    filter_catcher *catcher;

    /* Clear the old active_callback lists */
    for (func = 0; func < NUMBER_OF_FUNCTIONS; func++)
        bugle_list_clear(&active_callbacks[func]);

    for (i = bugle_list_head(&loaded_filters); i; i = bugle_list_next(i))
    {
        cur = (filter *) bugle_list_data(i);
        for (j = bugle_list_tail(&cur->callbacks); j; j = bugle_list_prev(j))
        {
            catcher = (filter_catcher *) bugle_list_data(j);
            if (cur->parent->active || catcher->inactive)
                bugle_list_append(&active_callbacks[catcher->function], catcher);
        }
    }
}

void *bugle_get_filter_set_call_state(function_call *call, filter_set *handle)
{
    if (handle && handle->call_state_offset >= 0)
        return (void *)(((char *) call->generic.user_data) + handle->call_state_offset);
    else
        return NULL;
}

void run_filters(function_call *call)
{
    bugle_list_node *i;
    filter_catcher *cur;
    callback_data data;

    /* FIXME: this lock effectively makes the entire capture process a
     * critical section, even though changes to active_callbacks and
     * active_dirty are rare. We would prefer a read-write lock or
     * a read-copy process or something.
     */
    bugle_thread_mutex_lock(&active_callbacks_mutex);
    if (active_dirty)
    {
        compute_active_callbacks();
        active_dirty = false;
    }

    call->generic.user_data = call_data;
    for (i = bugle_list_head(&active_callbacks[call->generic.id]); i; i = bugle_list_next(i))
    {
        cur = (filter_catcher *) bugle_list_data(i);
        data.call_data = bugle_get_filter_set_call_state(call, cur->parent->parent);
        data.filter_set_handle = cur->parent->parent;
        if (!(*cur->callback)(call, &data)) break;
    }

    while (bugle_list_head(&activations_deferred))
    {
        i = bugle_list_head(&activations_deferred);
        bugle_activate_filter_set_nolock((filter_set *) bugle_list_data(i));
        bugle_list_erase(&activations_deferred, i);
    }
    while (bugle_list_head(&deactivations_deferred))
    {
        i = bugle_list_head(&deactivations_deferred);
        bugle_deactivate_filter_set_nolock((filter_set *) bugle_list_data(i));
        bugle_list_erase(&deactivations_deferred, i);
    }
    bugle_thread_mutex_unlock(&active_callbacks_mutex);
}

filter_set *bugle_register_filter_set(const filter_set_info *info)
{
    filter_set *s;

    s = (filter_set *) bugle_malloc(sizeof(filter_set));
    s->name = info->name;
    s->help = info->help;
    bugle_list_init(&s->filters, false);
    s->load = info->load;
    s->unload = info->unload;
    s->activate = info->activate;
    s->deactivate = info->deactivate;
    s->variables = info->variables;
    s->loaded = false;
    s->active = false;
    s->dl_handle = current_dl_handle;
    if (info->call_state_space)
    {
        s->call_state_offset = call_data_size;
        call_data_size += info->call_state_space;
        call_data = bugle_realloc(call_data, call_data_size);
    }
    else s->call_state_offset = (ptrdiff_t) -1;

    bugle_list_append(&filter_sets, s);
    /* FIXME: dirty hack. To make sure that 'log' is loaded and loaded
     * first, make sure every filterset depends on it
     */
    if (strcmp(s->name, "log") != 0)
        bugle_register_filter_set_depends(s->name, "log");
    return s;
}

filter *bugle_register_filter(filter_set *handle, const char *name)
{
    filter *f;

    f = (filter *) bugle_malloc(sizeof(filter));
    f->name = name;
    f->parent = handle;
    bugle_list_init(&f->callbacks, true);
    bugle_list_append(&handle->filters, f);
    return f;
}

void bugle_register_filter_catches_function(filter *handle, budgie_function f,
                                            bool inactive,
                                            filter_callback callback)
{
    filter_catcher *cb;

    cb = (filter_catcher *) bugle_malloc(sizeof(filter_catcher));
    cb->parent = handle;
    cb->function = f;
    cb->inactive = inactive;
    cb->callback = callback;
    bugle_list_append(&handle->callbacks, cb);
}

void bugle_register_filter_catches(filter *handle, budgie_group g,
                                   bool inactive,
                                   filter_callback callback)
{
    budgie_function i;

    /* FIXME: there should be a way to speed this up */
    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        if (budgie_function_to_group[i] == g)
            bugle_register_filter_catches_function(handle, i, inactive, callback);
}

void bugle_register_filter_catches_all(filter *handle, bool inactive,
                                       filter_callback callback)
{
    budgie_function i;
    filter_catcher *cb;

    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        {
            cb = (filter_catcher *) bugle_malloc(sizeof(filter_catcher));
            cb->parent = handle;
            cb->function = i;
            cb->inactive = inactive;
            cb->callback = callback;
            bugle_list_append(&handle->callbacks, cb);
        }
}

void bugle_register_filter_depends(const char *after, const char *before)
{
    bugle_linked_list *deps;
    deps = (bugle_linked_list *) bugle_hash_get(&filter_dependencies, after);
    if (!deps)
    {
        deps = bugle_malloc(sizeof(bugle_linked_list));
        bugle_list_init(deps, true);
        bugle_hash_set(&filter_dependencies, after, deps);
    }
    bugle_list_append(deps, bugle_strdup(before));
}

void bugle_register_filter_set_depends(const char *base, const char *dep)
{
    bugle_list_append(&filter_set_dependencies[0], bugle_strdup(base));
    bugle_list_append(&filter_set_dependencies[1], bugle_strdup(dep));
}

bool bugle_filter_set_is_loaded(const filter_set *handle)
{
    assert(handle);
    return handle->loaded;
}

bool bugle_filter_set_is_active(const filter_set *handle)
{
    assert(handle);
    return handle->active;
}

filter_set *bugle_get_filter_set_handle(const char *name)
{
    bugle_list_node *i;
    filter_set *cur;

    for (i = bugle_list_head(&filter_sets); i; i = bugle_list_next(i))
    {
        cur = (filter_set *) bugle_list_data(i);
        if (strcmp(name, cur->name) == 0)
            return cur;
    }
    return NULL;
}

void *bugle_get_filter_set_symbol(filter_set *handle, const char *name)
{
    if (handle)
        return lt_dlsym(handle->dl_handle, name);
    else
    {
        void *h, *sym = NULL;
        h = lt_dlopen(NULL);
        if (h)
        {
            sym = lt_dlsym(h, name);
            lt_dlclose(h);
        }
        return sym;
    }
}

void bugle_filters_help(void)
{
    bugle_list_node *i;
    const filter_set_variable_info *j;
    filter_set *cur;

#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
    flockfile(stderr);
#endif
    fprintf(stderr, "Usage: BUGLE_CHAIN=<chain> LD_PRELOAD=libbugle.so <program> <args>\n");
    fprintf(stderr, "The following filter-sets are available:\n");
    for (i = bugle_list_head(&filter_sets); i; i = bugle_list_next(i))
    {
        cur = (filter_set *) bugle_list_data(i);
        if (cur->help)
            fprintf(stderr, "  %s: %s\n", cur->name, cur->help);
        j = cur->variables;
        for (j = cur->variables; j && j->name; j++)
            if (j->help)
            {
                const char *type_str = NULL;
                switch (j->type)
                {
                case FILTER_SET_VARIABLE_INT:
                case FILTER_SET_VARIABLE_UINT:
                case FILTER_SET_VARIABLE_POSITIVE_INT:
                    type_str = " (int)";
                    break;
                case FILTER_SET_VARIABLE_FLOAT:
                    type_str = " (float)";
                    break;
                case FILTER_SET_VARIABLE_BOOL:
                    type_str = " (bool)";
                    break;
                case FILTER_SET_VARIABLE_STRING:
                    type_str = " (string)";
                    break;
                case FILTER_SET_VARIABLE_KEY:
                    type_str = " (key)";
                case FILTER_SET_VARIABLE_CUSTOM:
                    type_str = "";
                    break;
                }
                fprintf(stderr, "    %s%s: %s\n",
                        j->name, type_str, j->help);
            }
    }
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
    funlockfile(stderr);
#endif
}
