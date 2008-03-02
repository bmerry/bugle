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
#define _POSIX_C_SOURCE 200112L /* For flockfile */
#define _BSD_SOURCE /* For finite() */
#define _XOPEN_SOURCE 600 /* For strtof */
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
#include <bugle/xevent.h>
#include <bugle/filters.h>
#include <bugle/log.h>
#include <bugle/linkedlist.h>
#include <bugle/hashtable.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include <budgie/addresses.h>
#include "budgielib/defines.h"
#include "xalloc.h"
#include "lock.h"

typedef struct
{
    filter *parent;
    budgie_function function;
    bool inactive;    /* True if callback should be called even when filterset is inactive */
    filter_callback callback;
} filter_catcher;

static linked_list filter_sets;
static linked_list added_filter_sets; /* Those specified in the config, plus dependents */

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
 * and deactivations within the same call, the order is not guaranteed
 * to be preserved (this may eventually be fixed). Similarly, the
 * result of bugle_filter_set_is_active will reflect the old values
 * until the end of the call.
 *
 * If a filter wishes to activate or deactivate a filter-set (its own
 * or another), it should call bugle_filter_set_activate_deferred
 * or bugle_filter_set_deactivate_deferred. This causes the change to
 * happen only after the current call has completed processing (which
 * removes the need to recompute the active filters while processing
 * them), and also prevents a self-deadlock.
 */
static linked_list loaded_filters;

/* FIXME: remove the dependence on defines.h */
static linked_list active_callbacks[FUNCTION_COUNT];
static bool active_dirty = false;
static linked_list activations_deferred;
static linked_list deactivations_deferred;
gl_lock_define_initialized(static, active_callbacks_mutex)

/* hash tables of linked lists of strings; A is the key, B is the linked list element */
static hash_table filter_orders;           /* A is called after B */
static hash_table filter_set_dependencies; /* A requires B */
static hash_table filter_set_orders;       /* A is initialised after B */

static lt_dlhandle current_dl_handle = NULL;

object_class *bugle_call_class;

static void filter_set_deactivate_nolock(filter_set *handle);

/*** Order management helper code ***/

typedef struct
{
    void *f;
    int valence;
} order_data;

static void register_order(hash_table *orders, const char *before, const char *after)
{
    linked_list *deps;
    deps = (linked_list *) bugle_hash_get(orders, after);
    if (!deps)
    {
        deps = XMALLOC(linked_list);
        bugle_list_init(deps, free);
        bugle_hash_set(orders, after, deps);
    }
    bugle_list_append(deps, xstrdup(before));
}

/* Returns true on success, false if there was a cycle.
 * - present: linked list of the binary items (not names); it is rewritten
 *   in place on success.
 * - order: name-name ordering dependencies, as a hash table of linked
 *   lists. If B is in A's linked list, then A must come before B.
 * - get_name: maps an item pointer to the name of the item
 */
static bool compute_order(linked_list *present,
                          hash_table *order,
                          const char *(*get_name)(void *))
{
    linked_list ordered;
    linked_list queue;
    linked_list *deps;
    hash_table byname;  /* maps names to order_data structs */
    const char *name;
    int count = 0;            /* count of outstanding items, to detect cycles */
    linked_list_node *i, *j;
    void *cur;
    order_data *info;

    bugle_list_init(&ordered, NULL);
    bugle_hash_init(&byname, free);
    for (i = bugle_list_head(present); i; i = bugle_list_next(i))
    {
        count++;
        info = XMALLOC(order_data);
        info->f = bugle_list_data(i);
        info->valence = 0;
        bugle_hash_set(&byname, get_name(info->f), info);
    }

    /* fill in valences */
    for (i = bugle_list_head(present); i; i = bugle_list_next(i))
    {
        cur = bugle_list_data(i);
        deps = (linked_list *) bugle_hash_get(order, get_name(cur));
        if (deps)
        {
            for (j = bugle_list_head(deps); j; j = bugle_list_next(j))
            {
                name = (const char *) bugle_list_data(j);
                info = (order_data *) bugle_hash_get(&byname, name);
                if (info) /* otherwise a non-existant object */
                    info->valence++;
            }
        }
    }

    /* prime the queue */
    bugle_list_init(&queue, NULL);
    for (i = bugle_list_head(present); i; i = bugle_list_next(i))
    {
        cur = (filter *) bugle_list_data(i);
        info = (order_data *) bugle_hash_get(&byname, get_name(cur));
        if (info->valence == 0)
            bugle_list_append(&queue, cur);
    }

    /* do a topological walk, starting at the back */
    while (bugle_list_head(&queue))
    {
        count--;
        cur = bugle_list_data(bugle_list_head(&queue));
        bugle_list_erase(&queue, bugle_list_head(&queue));
        deps = (linked_list *) bugle_hash_get(order, get_name(cur));
        if (deps)
        {
            for (j = bugle_list_head(deps); j; j = bugle_list_next(j))
            {
                name = (const char *) bugle_list_data(j);
                info = (order_data *) bugle_hash_get(&byname, name);
                if (info) /* otherwise a non-existent object */
                {
                    info->valence--;
                    if (info->valence == 0)
                        bugle_list_append(&queue, info->f);
                }
            }
        }
        bugle_list_prepend(&ordered, cur);
    }

    /* clean up */
    bugle_list_clear(&queue);
    bugle_hash_clear(&byname);
    if (count > 0)
    {
        bugle_list_clear(&ordered);
        return false;
    }
    else
    {
        void (*destructor)(void *);

        /* Some fiddling to prevent anything from being destroyed while we
         * shuffle the list structures around
         */
        destructor = present->destructor;
        present->destructor = NULL;
        bugle_list_clear(present);
        *present = ordered;
        present->destructor = destructor;
        return true;
    }
}

/*** End of order management helper code ***/

static void filter_free(filter *f)
{
    bugle_list_clear(&f->callbacks);
    free(f);
}

static void filter_set_free(filter_set *handle)
{
    if (handle->loaded)
    {
        handle->loaded = false;
        filter_set_deactivate_nolock(handle);
        if (handle->unload) (*handle->unload)(handle);

        bugle_list_clear(&handle->filters);
    }
}

static void filters_shutdown(void)
{
    linked_list_node *i;
    filter_set *s;
    budgie_function k;

    bugle_list_clear(&loaded_filters);
    for (k = 0; k < budgie_function_count(); k++)
        bugle_list_clear(&active_callbacks[k]);

    /* NB: this list runs backwards to obtain the correct shutdown order.
     * Don't try to turn it into a list destructor or the shutdown order
     * will be wrong.
     */
    for (i = bugle_list_tail(&added_filter_sets); i; i = bugle_list_prev(i))
    {
        s = (filter_set *) bugle_list_data(i);
        filter_set_free(s);
    }

    bugle_hash_clear(&filter_orders);
    bugle_hash_clear(&filter_set_dependencies);
    bugle_hash_clear(&filter_set_orders);
    bugle_list_clear(&added_filter_sets);
    bugle_list_clear(&filter_sets);
    bugle_object_class_free(bugle_call_class);
    lt_dlexit();
}

static int filter_library_init(const char *filename, lt_ptr data)
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

static void list_free(void *l)
{
    linked_list *list;

    list = (linked_list *) l;
    if (!list) return;
    bugle_list_clear(list);
    free(list);
}

/* Note: filters_initialise is called after initialise_real, which sets
 * up ltdl. Hence we don't call lt_dlinit.
 */
void filters_initialise(void)
{
    DIR *dir;
    const char *libdir;
    budgie_function f;

    bugle_list_init(&filter_sets, free);
    bugle_list_init(&added_filter_sets, NULL);
    bugle_list_init(&loaded_filters, NULL);
    for (f = 0; f < budgie_function_count(); f++)
        bugle_list_init(&active_callbacks[f], NULL);
    bugle_list_init(&activations_deferred, NULL);
    bugle_list_init(&deactivations_deferred, NULL);
    bugle_hash_init(&filter_orders, list_free);
    bugle_hash_init(&filter_set_dependencies, list_free);
    bugle_hash_init(&filter_set_orders, list_free);
    bugle_call_class = bugle_object_class_new(NULL);

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

    lt_dlforeachfile(libdir, filter_library_init, NULL);

    atexit(filters_shutdown);
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
                string_value = xstrdup(value);
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
static void filter_set_activate_nolock(filter_set *handle)
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
static void filter_set_deactivate_nolock(filter_set *handle)
{
    assert(handle);
    if (handle->active)
    {
        if (handle->deactivate) (*handle->deactivate)(handle);
        handle->active = false;
        active_dirty = true;
    }
}

void filter_set_add(filter_set *handle, bool activate)
{
    linked_list *deps;
    linked_list_node *i;
    filter_set *s;

    if (!handle->added)
    {
        handle->added = true;
        deps = (linked_list *) bugle_hash_get(&filter_set_dependencies, handle->name);
        if (deps)
        {
            for (i = bugle_list_head(deps); i; i = bugle_list_next(i))
            {
                s = bugle_filter_set_get_handle((const char *) bugle_list_data(i));
                if (!s)
                {
                    bugle_log_printf("filters", "load", BUGLE_LOG_ERROR,
                                     "filter-set %s depends on unknown filter-set %s",
                                     handle->name,
                                     ((const char *) bugle_list_data(i)));
                }
                filter_set_add(s, activate);
            }
        }
        bugle_list_append(&added_filter_sets, handle);
        handle->active = activate;
    }
}

static const char *get_name_filter_set(void *f)
{
    return ((filter_set *) f)->name;
}

/* Puts filter-sets into the proper order and initialises them */
void load_filter_sets(void)
{
    linked_list_node *i, *j;
    filter_set *handle;
    filter *f;

    compute_order(&added_filter_sets, &filter_set_orders, get_name_filter_set);
    for (i = bugle_list_head(&added_filter_sets); i; i = bugle_list_next(i))
    {
        handle = (filter_set *) bugle_list_data(i);
        if (handle->load && !(*handle->load)(handle))
        {
            bugle_log_printf(handle->name, "load", BUGLE_LOG_ERROR,
                             "failed to initialise filter-set %s", handle->name);
            exit(1);
        }
        handle->loaded = true;

        for (j = bugle_list_head(&handle->filters); j; j = bugle_list_next(j))
        {
            f = (filter *) bugle_list_data(j);
            bugle_list_append(&loaded_filters, f);
        }

        if (handle->active)
        {
            filter_set_activate_nolock(handle);
            active_dirty = true;
        }
    }
}

void filter_set_activate(filter_set *handle)
{
    gl_lock_lock(active_callbacks_mutex);
    filter_set_activate_nolock(handle);
    gl_lock_unlock(active_callbacks_mutex);
}

void filter_set_deactivate(filter_set *handle)
{
    gl_lock_lock(active_callbacks_mutex);
    filter_set_deactivate_nolock(handle);
    gl_lock_unlock(active_callbacks_mutex);
}

/* Note: these should be called only from within a callback, in which
 * case the lock is already held.
 * FIXME: a single queue should be used for activate and deactivate, to
 * allow for arbitrary ordering.
 */
void bugle_filter_set_activate_deferred(filter_set *handle)
{
    bugle_list_append(&activations_deferred, handle);
}

void bugle_filter_set_deactivate_deferred(filter_set *handle)
{
    bugle_list_append(&deactivations_deferred, handle);
}

static const char *filter_get_name(void *f)
{
    return ((filter *) f)->name;
}

static void filter_compute_order(void)
{
    bool success;

    success = compute_order(&loaded_filters, &filter_orders, filter_get_name);
    if (!success)
    {
        bugle_log("filters", "load", BUGLE_LOG_ERROR,
                  "cyclic dependency between filters");
        exit(1);
    }
}

static void set_bypass(void)
{
    linked_list_node *i, *j;
    int k;
    bool *bypass;
    filter *cur;
    filter_catcher *catcher;

    bypass = XNMALLOC(budgie_function_count(), bool);
    /* We use this temporary instead of modifying values directly, because
     * we don't want other threads to see intermediate values.
     */
    memset(bypass, 1, budgie_function_count() * sizeof(bool));
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
    for (k = 0; k < budgie_function_count(); k++)
        budgie_function_set_bypass(k, bypass[k]);
    free(bypass);
}

void filters_finalise(void)
{
    load_filter_sets();
    filter_compute_order();
    set_bypass();
}

/* Note: caller must take mutexes */
static void compute_active_callbacks(void)
{
    linked_list_node *i, *j;
    filter *cur;
    budgie_function func;
    filter_catcher *catcher;

    /* Clear the old active_callback lists */
    for (func = 0; func < budgie_function_count(); func++)
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

void filters_run(function_call *call)
{
    linked_list_node *i;
    filter_catcher *cur;
    callback_data data;

    /* FIXME: this lock effectively makes the entire capture process a
     * critical section, even though changes to active_callbacks and
     * active_dirty are rare. We would prefer a read-write lock or
     * a read-copy process or something.
     */
    gl_lock_lock(active_callbacks_mutex);
    if (active_dirty)
    {
        compute_active_callbacks();
        active_dirty = false;
    }

    data.call_object = bugle_object_new(bugle_call_class, NULL, true);
    for (i = bugle_list_head(&active_callbacks[call->generic.id]); i; i = bugle_list_next(i))
    {
        cur = (filter_catcher *) bugle_list_data(i);
        data.filter_set_handle = cur->parent->parent;
        if (!(*cur->callback)(call, &data)) break;
    }
    bugle_object_free(data.call_object);

    while (bugle_list_head(&activations_deferred))
    {
        i = bugle_list_head(&activations_deferred);
        filter_set_activate_nolock((filter_set *) bugle_list_data(i));
        bugle_list_erase(&activations_deferred, i);
    }
    while (bugle_list_head(&deactivations_deferred))
    {
        i = bugle_list_head(&deactivations_deferred);
        filter_set_deactivate_nolock((filter_set *) bugle_list_data(i));
        bugle_list_erase(&deactivations_deferred, i);
    }
    gl_lock_unlock(active_callbacks_mutex);
}

filter_set *bugle_filter_set_new(const filter_set_info *info)
{
    filter_set *s;

    s = XMALLOC(filter_set);
    s->name = info->name;
    s->help = info->help;
    bugle_list_init(&s->filters, (void (*)(void *)) filter_free);
    s->load = info->load;
    s->unload = info->unload;
    s->activate = info->activate;
    s->deactivate = info->deactivate;
    s->variables = info->variables;
    s->loaded = false;
    s->active = false;
    s->added = false;
    s->dl_handle = current_dl_handle;

    bugle_list_append(&filter_sets, s);
    /* FIXME: dirty hack. To make sure that 'log' is loaded and loaded
     * first, make sure every filter-set depends on it
     */
    if (strcmp(s->name, "log") != 0)
    {
        bugle_filter_set_depends(s->name, "log");
        bugle_filter_set_order("log", s->name);
    }
    return s;
}

filter *bugle_filter_new(filter_set *handle, const char *name)
{
    filter *f;

    f = XMALLOC(filter);
    f->name = name;
    f->parent = handle;
    bugle_list_init(&f->callbacks, free);
    bugle_list_append(&handle->filters, f);
    return f;
}

void bugle_filter_catches_function_id(filter *handle, budgie_function id,
                                      bool inactive,
                                      filter_callback callback)
{
    filter_catcher *cb;

    cb = XMALLOC(filter_catcher);
    cb->parent = handle;
    cb->function = id;
    cb->inactive = inactive;
    cb->callback = callback;
    bugle_list_append(&handle->callbacks, cb);
}

void bugle_filter_catches_function(filter *handle, const char *f,
                                   bool inactive,
                                   filter_callback callback)
{
    budgie_function id;

    id = budgie_function_id(f);
    if (id != NULL_FUNCTION)
        bugle_filter_catches_function_id(handle, id, inactive, callback);
    else
        bugle_log_printf(handle->parent->name, handle->name, BUGLE_LOG_WARNING,
                         "Attempt to catch unknown function %s", f);
}

void bugle_filter_catches(filter *handle, const char *group,
                          bool inactive,
                          filter_callback callback)
{
    budgie_function i;
    budgie_group g;

    i = budgie_function_id(group);
    if (i == NULL_FUNCTION)
    {
        bugle_log_printf(handle->parent->name, handle->name, BUGLE_LOG_WARNING,
                         "Attempt to catch unknown function %s", group);
        return;
    }
    g = budgie_function_group(i);
    /* FIXME: there should be a way to speed this up */
    for (i = 0; i < budgie_function_count(); i++)
        if (budgie_function_group(i) == g)
            bugle_filter_catches_function_id(handle, i, inactive, callback);
}

void bugle_filter_catches_all(filter *handle, bool inactive,
                              filter_callback callback)
{
    budgie_function i;
    filter_catcher *cb;

    for (i = 0; i < budgie_function_count(); i++)
    {
        cb = XMALLOC(filter_catcher);
        cb->parent = handle;
        cb->function = i;
        cb->inactive = inactive;
        cb->callback = callback;
        bugle_list_append(&handle->callbacks, cb);
    }
}

void bugle_filter_order(const char *before, const char *after)
{
    register_order(&filter_orders, before, after);
}

void bugle_filter_set_depends(const char *base, const char *dep)
{
    register_order(&filter_set_dependencies, dep, base);
    register_order(&filter_set_orders, dep, base);
}

void bugle_filter_set_order(const char *before, const char *after)
{
    register_order(&filter_set_orders, before, after);
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

filter_set *bugle_filter_set_get_handle(const char *name)
{
    linked_list_node *i;
    filter_set *cur;

    for (i = bugle_list_head(&filter_sets); i; i = bugle_list_next(i))
    {
        cur = (filter_set *) bugle_list_data(i);
        if (strcmp(name, cur->name) == 0)
            return cur;
    }
    return NULL;
}

void *bugle_filter_set_get_symbol(filter_set *handle, const char *name)
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

void filters_help(void)
{
    linked_list_node *i;
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
