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
#include "src/utils.h"
#include "src/glfuncs.h"
#include "filters.h"
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
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

static bugle_linked_list filter_sets;
static bugle_linked_list active_filters;
/* hash table of linked lists of strings */
static bugle_hash_table filter_dependencies;
static bugle_linked_list filter_set_dependencies[2];
static bool dirty_active = false;
static void *call_data = NULL;
static size_t call_data_size = 0; /* FIXME: turn into an object */

/* To speed things up, each filter lists the functions that it catches.
 * Functions that are not caught at all are skipped over. Note that the
 * catch hints are only advisory, and filters must explicitly ignore
 * other functions.
 */
static size_t function_refcount[NUMBER_OF_FUNCTIONS];
/* This is incremented for each function that wants to catch everything.
 * It is statically initialised to 1, because otherwise the dynamic
 * initialisation may never run at all.
 */
static size_t all_refcount = 1;
static pthread_mutex_t refcount_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *current_dl_handle = NULL;

/* FIXME: use dlclose on the modules */
void destroy_filters(void)
{
    bugle_list_node *i, *j;
    filter_set *s;
    filter *f;
    bugle_linked_list *dep;

    bugle_list_clear(&filter_set_dependencies[0], true);
    bugle_list_clear(&filter_set_dependencies[1], true);
    bugle_list_clear(&active_filters, false);
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
                    bugle_list_clear(dep, true);
                    free(dep);
                }
                bugle_list_clear(&f->catches, false);
                free(f->name);
                free(f);
            }
            bugle_list_clear(&s->filters, false);
        }
        free(s->name);
        free(s);
    }
    bugle_list_clear(&filter_sets, false);
    bugle_hash_clear(&filter_dependencies, false);
}

bool check_skip(budgie_function f)
{
    bool ans;

    pthread_mutex_lock(&refcount_mutex);
    ans = !(all_refcount || function_refcount[f]);
    pthread_mutex_unlock(&refcount_mutex);
    /* if (ans) printf("Skipping %s\n", budgie_function_table[f].name); */
    return ans;
}

void initialise_filters(void)
{
    DIR *dir;
    struct dirent *ent;
    char *full_name;
    size_t len;
    void *handle;
    void (*init)(void);
    const char *libdir;

    bugle_list_init(&filter_sets);
    bugle_list_init(&active_filters);
    bugle_hash_init(&filter_dependencies);
    bugle_list_init(&filter_set_dependencies[0]);
    bugle_list_init(&filter_set_dependencies[1]);
    memset(function_refcount, 0, sizeof(function_refcount));

    libdir = getenv("BUGLE_FILTER_DIR");
    if (!libdir) libdir = PKGLIBDIR;
    dir = opendir(libdir);
    if (!dir)
    {
        fprintf(stderr, "failed to open %s: %s", libdir, strerror(errno));
        exit(1);
    }

    while ((ent = readdir(dir)) != NULL)
    {
        len = strlen(ent->d_name);
        if (len < 3) continue;
        if (strcmp(ent->d_name + len - 3, ".so") != 0) continue;
        full_name = (char *) bugle_malloc(strlen(libdir) + strlen(ent->d_name) + 2);
        sprintf(full_name, "%s/%s", libdir, ent->d_name);
        handle = dlopen(full_name, RTLD_LAZY);
        if (handle == NULL) continue;
        init = (void (*)(void)) dlsym(handle, "bugle_initialise_filter_library");
        if (init == NULL)
        {
            fprintf(stderr, "Warning: library %s did not export initialisation symbol\n",
                    ent->d_name);
            continue;
        }
        current_dl_handle = handle;
        (*init)();
        current_dl_handle = NULL;
        free(full_name);
    }

    closedir(dir);
    atexit(destroy_filters);

    all_refcount--; /* Cancel out the static initialisation to 1 */
}

bool filter_set_command(filter_set *handle, const char *name, const char *value)
{
    if (!handle->command_handler) return false;
    return (*handle->command_handler)(handle, name, value);
}

static void enable_filter_set_r(filter_set *handle)
{
    bugle_list_node *i, *j;
    filter_set *s;
    filter *f;

    if (!handle->enabled)
    {
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
        /* deps */
        for (i = bugle_list_head(&filter_set_dependencies[0]),
             j = bugle_list_head(&filter_set_dependencies[1]);
             i != NULL;
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
        for (i = bugle_list_head(&handle->filters); i != NULL; i = bugle_list_next(i))
        {
            f = (filter *) bugle_list_data(i);
            bugle_list_append(&active_filters, f);
            if (f->catches_all) all_refcount++;
            for (j = bugle_list_head(&f->catches); j != NULL; j = bugle_list_next(j))
                (*(size_t *) bugle_list_data(j))++;
        }
        dirty_active = true;
    }
}

void bugle_enable_filter_set(filter_set *handle)
{
    pthread_mutex_lock(&refcount_mutex);
    enable_filter_set_r(handle);
    pthread_mutex_unlock(&refcount_mutex);
}

static void disable_filter_set_r(filter_set *handle)
{
    bugle_list_node *i, *j, *k;
    filter_set *s;
    filter *f;

    if (handle->enabled)
    {
        handle->enabled = false;
        /* reverse deps */
        for (i = bugle_list_head(&filter_set_dependencies[0]),
             j = bugle_list_head(&filter_set_dependencies[1]);
             i != NULL;
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
            {
                for (k = bugle_list_head(&f->catches); k != NULL; k = bugle_list_next(k))
                    (*(size_t *) bugle_list_data(k))--;
                if (f->catches_all) all_refcount--;
                bugle_list_erase(&active_filters, i, false);
            }
            i = j;
        }
        dirty_active = true;
    }
}

void bugle_disable_filter_set(filter_set *handle)
{
    pthread_mutex_lock(&refcount_mutex);
    disable_filter_set_r(handle);
    pthread_mutex_unlock(&refcount_mutex);
}

void repair_filter_order(void)
{
    bugle_list_node *i, *j;
    bugle_linked_list active; /* replacement for active_filters */
    bugle_linked_list *deps;
    bugle_linked_list queue;
    bugle_hash_table names;   /* maps filter names to filters */
    bugle_hash_table valence; /* for re-ordering */
    filter *cur;
    /* We encode integers into pointers as base + n, so base is arbitrary
     * but must be non-NULL (so that NULL is different from 0)
     */
    char base[] = "", *d;
    const char *name;
    int count = 0; /* checks that everything made it without cycles */

    bugle_list_init(&active);
    bugle_hash_init(&valence);
    bugle_hash_init(&names);
    /* initialise name table, and set valence table to all 0's */
    for (i = bugle_list_head(&active_filters); i; i = bugle_list_next(i))
    {
        count++;
        cur = (filter *) bugle_list_data(i);
        bugle_hash_set(&names, cur->name, cur);
        bugle_hash_set(&valence, cur->name, base);
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
                d = bugle_hash_get(&valence, name);
                if (d) /* otherwise a non-existant filter */
                {
                    d++;
                    bugle_hash_set(&valence, name, d);
                }
            }
        }
    }
    /* prime the queue */
    bugle_list_init(&queue);
    for (i = bugle_list_head(&active_filters); i; i = bugle_list_next(i))
    {
        cur = (filter *) bugle_list_data(i);
        if (bugle_hash_get(&valence, cur->name) == base)
            bugle_list_append(&queue, cur);
    }
    /* do a topological walk, starting at the back */
    while (bugle_list_head(&queue))
    {
        count--;
        cur = (filter *) bugle_list_data(bugle_list_head(&queue));
        bugle_list_erase(&queue, bugle_list_head(&queue), false);
        bugle_list_prepend(&active, cur);
        deps = (bugle_linked_list *) bugle_hash_get(&filter_dependencies, cur->name);
        if (deps)
        {
            for (j = bugle_list_head(deps); j; j = bugle_list_next(j))
            {
                name = (const char *) bugle_list_data(j);
                d = bugle_hash_get(&valence, name);
                if (d) /* otherwise a non-existant filter */
                {
                    d--;
                    bugle_hash_set(&valence, name, d);
                    if (d == base)
                        bugle_list_append(&queue, bugle_hash_get(&names, name));
                }
            }
        }
    }
    if (count > 0)
    {
        fprintf(stderr, "cyclic dependency between filters, aborting\n");
        exit(1);
    }
    /* replace the old */
    bugle_list_clear(&active_filters, false);
    memcpy(&active_filters, &active, sizeof(active));
    /* clean up */
    bugle_list_clear(&queue, false);
    bugle_hash_clear(&valence, false);
    bugle_hash_clear(&names, false);
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
    filter *cur;
    callback_data data;

    if (dirty_active)
    {
        dirty_active = false;
        repair_filter_order();
    }

    call->generic.user_data = call_data;
    for (i = bugle_list_head(&active_filters); i != NULL; i = bugle_list_next(i))
    {
        cur = (filter *) bugle_list_data(i);
        data.call_data = bugle_get_filter_set_call_state(call, cur->parent);
        if (!(*cur->callback)(call, &data)) break;
    }
}

filter_set *bugle_register_filter_set(const filter_set_info *info)
{
    filter_set *s;

    s = (filter_set *) bugle_malloc(sizeof(filter_set));
    s->name = bugle_strdup(info->name);
    bugle_list_init(&s->filters);
    s->init = info->init;
    s->done = info->done;
    s->command_handler = info->command_handler;
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

filter *bugle_register_filter(filter_set *handle, const char *name,
                              filter_callback callback)
{
    filter *f;

    f = (filter *) bugle_malloc(sizeof(filter));
    f->name = bugle_strdup(name);
    f->callback = callback;
    f->parent = handle;
    bugle_list_init(&f->catches);
    f->catches_all = false;
    bugle_list_append(&handle->filters, f);
    return f;
}

void bugle_register_filter_catches(filter *handle, budgie_function f)
{
    budgie_function i;

    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        if (bugle_gl_function_table[i].canonical == bugle_gl_function_table[f].canonical)
            bugle_list_append(&handle->catches, &function_refcount[i]);
}

void bugle_register_filter_catches_all(filter *handle)
{
    handle->catches_all = true;
}

void bugle_register_filter_depends(const char *after, const char *before)
{
    bugle_linked_list *deps;
    deps = (bugle_linked_list *) bugle_hash_get(&filter_dependencies, after);
    if (!deps)
    {
        deps = bugle_malloc(sizeof(bugle_linked_list));
        bugle_list_init(deps);
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

    for (i = bugle_list_head(&filter_sets); i != NULL; i = bugle_list_next(i))
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
        return dlsym(handle->dl_handle, name);
    else
    {
        void *h, *sym = NULL;
        h = dlopen(NULL, RTLD_LAZY);
        if (h)
        {
            sym = dlsym(h, name);
            dlclose(h);
        }
        return sym;
    }
}
