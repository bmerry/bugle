/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
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
#define _POSIX_SOURCE
#include "src/utils.h"
#include "src/glfuncs.h"
#include "filters.h"
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
    filter_callback callback;
} filter_catcher;

static bugle_linked_list filter_sets;

/* When filter-sets are activated and deactivated, the active_filters
 * list is updated to contain pointers to the active filters, and
 * active_dirty is set to true. Note that active_filters is an
 * _un_sorted list. When the next call comes in, it notices that
 * active_dirty is true and does a topological sort to recreate the
 * active_callbacks array.
 *
 * Each active_callback list entry points to a filter_catcher
 * in the original filter structure.
 *
 * The locking gets a little hairy, because we have to avoid a deadlock
 * when a filter (viz the debugger) enables or disables filtersets.
 * There are three usage scenarios:
 * 1. Modify active_filters and set active_dirty.
 *    Lock active_filters_mutex.
 * 2. Modify active_callbacks and clear active_dirty.
 *    Lock active_callbacks_mutex and active_filters_mutex, in that order.
 * 3. Read from active_callbacks.
 *    Lock active_callbacks_mutex.
 * The lock order in scenario 2 is critical, because scenario 1 can occur
 * inside scenario 3.
 */
static bugle_linked_list active_filters;
static bugle_linked_list active_callbacks[NUMBER_OF_FUNCTIONS];
static bool active_dirty = false;
static bugle_thread_mutex_t active_filters_mutex = BUGLE_THREAD_MUTEX_INITIALIZER;
static bugle_thread_mutex_t active_callbacks_mutex = BUGLE_THREAD_MUTEX_INITIALIZER;

/* hash table of linked lists of strings */
static bugle_hash_table filter_dependencies;
static bugle_linked_list filter_set_dependencies[2];
static void *call_data = NULL;
static size_t call_data_size = 0; /* FIXME: turn into an object */

static lt_dlhandle current_dl_handle = NULL;

static void destroy_filters(void *dummy)
{
    bugle_list_node *i, *j;
    filter_set *s;
    filter *f;
    budgie_function k;
    bugle_linked_list *dep;

    bugle_list_clear(&filter_set_dependencies[0]);
    bugle_list_clear(&filter_set_dependencies[1]);
    bugle_list_clear(&active_filters);
    for (k = 0; k < NUMBER_OF_FUNCTIONS; k++)
        bugle_list_clear(&active_callbacks[k]);
    for (i = bugle_list_head(&filter_sets); i; i = bugle_list_next(i))
    {
        s = (filter_set *) bugle_list_data(i);
        if (s->initialised)
        {
            if (s->done)
                (*s->done)(s);
            for (j = bugle_list_head(&s->filters); j; j = bugle_list_next(j))
            {
                f = (filter *) bugle_list_data(j);
                dep = (bugle_linked_list *) bugle_hash_get(&filter_dependencies, f->name);
                if (dep)
                {
                    bugle_list_clear(dep);
                    free(dep);
                }
                bugle_list_clear(&f->callbacks);
                free(f);
            }
            bugle_list_clear(&s->filters);
        }
        free(s);
    }
    bugle_list_clear(&filter_sets);
    bugle_hash_clear(&filter_dependencies);

    lt_dlexit();
}

static int initialise_filter(const char *filename, lt_ptr data)
{
    lt_dlhandle handle;
    void (*init)(void);

    handle = lt_dlopenext(filename);
    if (handle == NULL) return 0;

    init = (void (*)(void)) lt_dlsym(handle, "bugle_initialise_filter_library");
    if (init == NULL)
    {
        fprintf(stderr, "Warning: library %s did not export initialisation symbol\n",
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
    bugle_list_init(&active_filters, false);
    for (f = 0; f < NUMBER_OF_FUNCTIONS; f++)
        bugle_list_init(&active_callbacks[f], false);
    bugle_hash_init(&filter_dependencies, false);
    bugle_list_init(&filter_set_dependencies[0], true);
    bugle_list_init(&filter_set_dependencies[1], true);

    libdir = getenv("BUGLE_FILTER_DIR");
    if (!libdir) libdir = PKGLIBDIR;
    dir = opendir(libdir);
    if (!dir)
    {
        fprintf(stderr, "failed to open %s: %s", libdir, strerror(errno));
        exit(1);
    }
    closedir(dir);

    lt_dlforeachfile(libdir, initialise_filter, NULL);

    bugle_atexit(destroy_filters, NULL);
}

bool filter_set_variable(filter_set *handle, const char *name, const char *value)
{
    const filter_set_variable_info *v;
    bool bool_value;
    long int_value;
    char *string_value;
    char *end;
    void *value_ptr = NULL;

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
                    fprintf(stderr, "Expected 1|0|yes|no|true|false for %s in filter-set %s\n",
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
                    fprintf(stderr, "Expected an integer for %s in filter-set %s\n",
                            name, handle->name);
                    return false;
                }
                if (v->type == FILTER_SET_VARIABLE_UINT && int_value < 0)
                {
                    fprintf(stderr, "Expected a non-negative integer for %s in filter-set %s\n",
                            name, handle->name);
                    return false;
                }
                else if (v->type == FILTER_SET_VARIABLE_POSITIVE_INT && int_value <= 0)
                {
                    fprintf(stderr, "Expected a positive integer for %s in filter-set %s\n",
                            name, handle->name);
                    return false;
                }
                value_ptr = &int_value;
                break;
            case FILTER_SET_VARIABLE_STRING:
                string_value = bugle_strdup(value);
                value_ptr = &string_value;
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
                    case FILTER_SET_VARIABLE_STRING:
                        if (*(char **) v->value)
                            free(*(char **) v->value);
                        *(char **) v->value = string_value;
                        break;
                    }
                }
                return true;
            }
        }
    }
    fprintf(stderr, "Unknown variable %s in filter-set %s\n",
            name, handle->name);
    return false;
}

static void enable_filter_set_r(filter_set *handle)
{
    bugle_list_node *i, *j;
    filter_set *s;
    filter *f;

    if (!handle->enabled)
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
                    fprintf(stderr, "filter-set %s depends on unknown filter-set %s\n",
                            ((const char *) bugle_list_data(i)),
                            ((const char *) bugle_list_data(j)));
                    exit(1);
                }
                enable_filter_set_r(s);
            }
        }
        /* Initialisation */
        if (!handle->initialised)
        {
            if (!(*handle->init)(handle))
            {
                fprintf(stderr, "Failed to initialise filter-set %s\n", handle->name);
                exit(1);
            }
            handle->initialised = true;
        }
        handle->enabled = true;
        for (i = bugle_list_head(&handle->filters); i; i = bugle_list_next(i))
        {
            f = (filter *) bugle_list_data(i);
            bugle_list_append(&active_filters, f);
        }
        active_dirty = true;
    }
}

void bugle_enable_filter_set(filter_set *handle)
{
    bugle_thread_mutex_lock(&active_filters_mutex);
    enable_filter_set_r(handle);
    bugle_thread_mutex_unlock(&active_filters_mutex);
}

static void disable_filter_set_r(filter_set *handle)
{
    bugle_list_node *i, *j;
    filter_set *s;
    filter *f;

    if (handle->enabled)
    {
        handle->enabled = false;
        /* reverse deps */
        for (i = bugle_list_head(&filter_set_dependencies[0]),
             j = bugle_list_head(&filter_set_dependencies[1]);
             i;
             i = bugle_list_next(i), j = bugle_list_next(j))
        {
            if (strcmp(handle->name, (const char *) bugle_list_data(j)) == 0)
            {
                s = bugle_get_filter_set_handle((const char *) bugle_list_data(i));
                disable_filter_set_r(s);
            }
        }
        i = bugle_list_head(&active_filters);
        while (i)
        {
            j = bugle_list_next(i);
            f = (filter *) bugle_list_data(i);
            if (f->parent == handle)
                bugle_list_erase(&active_filters, i);
            i = j;
        }
        active_dirty = true;
    }
}

void bugle_disable_filter_set(filter_set *handle)
{
    bugle_thread_mutex_lock(&active_filters_mutex);
    disable_filter_set_r(handle);
    bugle_thread_mutex_unlock(&active_filters_mutex);
}

typedef struct
{
    filter *f;
    int valence;
} repair_info;

/* Note: caller must take mutexes */
void repair_filter_order(void)
{
    bugle_list_node *i, *j;
    bugle_linked_list *deps;
    bugle_linked_list queue;
    bugle_hash_table info;    /* table of repair_info structs for each filter */
    repair_info *cur_info;
    filter *cur;
    const char *name;
    int count = 0; /* checks that everything made it without cycles */
    budgie_function func;
    filter_catcher *catcher;

    /* Clear the old active_callback lists */
    for (func = 0; func < NUMBER_OF_FUNCTIONS; func++)
        bugle_list_clear(&active_callbacks[func]);

    bugle_hash_init(&info, true);
    /* initialise info table */
    for (i = bugle_list_head(&active_filters); i; i = bugle_list_next(i))
    {
        count++;
        cur_info = (repair_info *) bugle_malloc(sizeof(repair_info));
        cur_info->f = (filter *) bugle_list_data(i);
        cur_info->valence = 0;
        bugle_hash_set(&info, cur_info->f->name, cur_info);
    }
    /* fill in valences */
    for (i = bugle_list_head(&active_filters); i; i = bugle_list_next(i))
    {
        cur = (filter *) bugle_list_data(i);
        deps = (bugle_linked_list *) bugle_hash_get(&filter_dependencies, cur->name);
        if (deps)
        {
            for (j = bugle_list_head(deps); j; j = bugle_list_next(j))
            {
                name = (const char *) bugle_list_data(j);
                cur_info = (repair_info *) bugle_hash_get(&info, name);
                if (cur_info) /* otherwise a non-active filter */
                    cur_info->valence++;
            }
        }
    }
    /* prime the queue */
    bugle_list_init(&queue, false);
    for (i = bugle_list_head(&active_filters); i; i = bugle_list_next(i))
    {
        cur = (filter *) bugle_list_data(i);
        cur_info = (repair_info *) bugle_hash_get(&info, cur->name);
        if (cur_info->valence == 0)
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
                cur_info = (repair_info *) bugle_hash_get(&info, name);
                if (cur_info) /* otherwise a non-active filter */
                {
                    cur_info->valence--;
                    if (cur_info->valence == 0)
                        bugle_list_append(&queue, cur_info->f);
                }
            }
        }
        for (j = bugle_list_tail(&cur->callbacks); j; j = bugle_list_prev(j))
        {
            catcher = (filter_catcher *) bugle_list_data(j);
            bugle_list_prepend(&active_callbacks[catcher->function], catcher);
        }
    }
    if (count > 0)
    {
        fprintf(stderr, "cyclic dependency between filters, aborting\n");
        exit(1);
    }
    /* clean up */
    bugle_list_clear(&queue);
    bugle_hash_clear(&info);
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
     * active_dirty are rare. We would prefer a read-write lock.
     */
    bugle_thread_mutex_lock(&active_callbacks_mutex);
    bugle_thread_mutex_lock(&active_filters_mutex);
    if (active_dirty)
    {
        repair_filter_order();
        active_dirty = false;
    }
    bugle_thread_mutex_unlock(&active_filters_mutex);

    call->generic.user_data = call_data;
    for (i = bugle_list_head(&active_callbacks[call->generic.id]); i; i = bugle_list_next(i))
    {
        cur = (filter_catcher *) bugle_list_data(i);
        data.call_data = bugle_get_filter_set_call_state(call, cur->parent->parent);
        if (!(*cur->callback)(call, &data)) break;
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
    s->init = info->init;
    s->done = info->done;
    s->variables = info->variables;
    s->initialised = false;
    s->enabled = false;
    s->dl_handle = current_dl_handle;
    if (info->call_state_space)
    {
        s->call_state_offset = call_data_size;
        call_data_size += info->call_state_space;
        call_data = bugle_realloc(call_data, call_data_size);
    }
    else s->call_state_offset = (ptrdiff_t) -1;

    bugle_list_append(&filter_sets, s);
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

void bugle_register_filter_catches(filter *handle, budgie_group g,
                                   filter_callback callback)
{
    budgie_function i;
    filter_catcher *cb;

    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        if (budgie_function_to_group[i] == g)
        {
            cb = (filter_catcher *) bugle_malloc(sizeof(filter_catcher));
            cb->parent = handle;
            cb->function = i;
            cb->callback = callback;
            bugle_list_append(&handle->callbacks, cb);
        }
}

void bugle_register_filter_catches_all(filter *handle, filter_callback callback)
{
    budgie_function i;
    filter_catcher *cb;

    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        {
            cb = (filter_catcher *) bugle_malloc(sizeof(filter_catcher));
            cb->parent = handle;
            cb->function = i;
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

bool bugle_filter_set_is_enabled(const filter_set *handle)
{
    assert(handle);
    return handle->enabled;
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

    flockfile(stderr);
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
                    type_str = "int";
                    break;
                case FILTER_SET_VARIABLE_BOOL:
                    type_str = "bool";
                    break;
                case FILTER_SET_VARIABLE_STRING:
                    type_str = "string";
                    break;
                }
                fprintf(stderr, "    %s (%s): %s\n",
                        j->name, type_str, j->help);
            }
    }
    funlockfile(stderr);
}
