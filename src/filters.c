#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/types.h"
#include "src/utils.h"
#include "filters.h"
#include "linkedlist.h"
#include "hashtable.h"
#include "src/safemem.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dlfcn.h>
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

static linked_list filter_sets;
static linked_list active_filters;
/* hash table of linked lists of strings */
static hash_table filter_dependencies;
static linked_list filter_set_dependencies[2];
static bool dirty_active = false;
static void *call_data = NULL;
static size_t call_data_size = 0;

static void *current_dl_handle = NULL;

/* FIXME: use dlclose on the modules */
void destroy_filters(void)
{
    list_node *i, *j;
    filter_set *s;
    filter *f;
    linked_list *dep;

    list_clear(&filter_set_dependencies[0], true);
    list_clear(&filter_set_dependencies[1], true);
    list_clear(&active_filters, false);
    for (i = list_head(&filter_sets); i; i = list_next(i))
    {
        s = (filter_set *) list_data(i);
        if (s->initialised)
        {
            if (s->done)
                (*s->done)(s);
            for (j = list_head(&s->filters); j; j = list_next(j))
            {
                f = (filter *) list_data(j);
                dep = (linked_list *) hash_get(&filter_dependencies, f->name);
                if (dep)
                {
                    list_clear(dep, true);
                    free(dep);
                }
                free(f->name);
                free(f);
            }
            list_clear(&s->filters, false);
        }
        free(s->name);
        free(s);
    }
    list_clear(&filter_sets, false);
    hash_clear(&filter_dependencies, false);
}

void initialise_filters(void)
{
    DIR *dir;
    struct dirent *ent;
    char *full_name;
    size_t len;
    void *handle;
    void (*init)(void);

    list_init(&filter_sets);
    list_init(&active_filters);
    hash_init(&filter_dependencies);
    list_init(&filter_set_dependencies[0]);
    list_init(&filter_set_dependencies[1]);

    dir = opendir(PKGLIBDIR);
    if (!dir)
    {
        perror("failed to open " PKGLIBDIR);
        exit(1);
    }

    while ((ent = readdir(dir)) != NULL)
    {
        len = strlen(ent->d_name);
        if (len < 3) continue;
        if (strcmp(ent->d_name + len - 3, ".so") != 0) continue;
        full_name = (char *) xmalloc(strlen(PKGLIBDIR) + strlen(ent->d_name) + 2);
        sprintf(full_name, PKGLIBDIR "/%s", ent->d_name);
        handle = dlopen(full_name, RTLD_LAZY);
        if (handle == NULL) continue;
        init = (void (*)(void)) dlsym(handle, "initialise_filter_library");
        if (init == NULL) continue;
        current_dl_handle = handle;
        (*init)();
        current_dl_handle = NULL;
        free(full_name);
    }

    closedir(dir);
    atexit(destroy_filters);
}

bool set_filter_set_variable(filter_set *handle, const char *name, const char *value)
{
    if (!handle->set_variable) return false;
    return (*handle->set_variable)(handle, name, value);
}

void enable_filter_set(filter_set *handle)
{
    list_node *i, *j;

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
        for (i = list_head(&filter_set_dependencies[0]),
             j = list_head(&filter_set_dependencies[1]);
             i != NULL;
             i = list_next(i), j = list_next(j))
        {
            if (strcmp(handle->name, (const char *) list_data(i)) == 0)
                enable_filter_set(get_filter_set_handle((const char *) list_data(j)));
        }
        for (i = list_head(&handle->filters); i != NULL; i = list_next(i))
            list_append(&active_filters, list_data(i));
        dirty_active = true;
    }
}

void repair_filter_order(void)
{
    list_node *i, *j;
    linked_list active; /* replacement for active_filters */
    linked_list *deps;
    linked_list queue;
    hash_table names;   /* maps filter names to filters */
    hash_table valence; /* for re-ordering */
    filter *cur;
    /* We encode integers into pointers as base + n, so base is arbitrary
     * but must be non-NULL (so that NULL is different from 0)
     */
    char base[] = "", *d;
    const char *name;
    int count = 0; /* checks that everything made it without cycles */

    list_init(&active);
    hash_init(&valence);
    hash_init(&names);
    /* initialise name table, and set valence table to all 0's */
    for (i = list_head(&active_filters); i; i = list_next(i))
    {
        count++;
        cur = (filter *) list_data(i);
        hash_set(&names, cur->name, cur);
        hash_set(&valence, cur->name, base);
    }
    /* fill in valences */
    for (i = list_head(&active_filters); i; i = list_next(i))
    {
        cur = (filter *) list_data(i);
        deps = (linked_list *) hash_get(&filter_dependencies, cur->name);
        if (deps)
        {
            for (j = list_head(deps); j; j = list_next(j))
            {
                name = (const char *) list_data(j);
                d = hash_get(&valence, name);
                if (d) /* otherwise a non-existant filter */
                {
                    d++;
                    hash_set(&valence, name, d);
                }
            }
        }
    }
    /* prime the queue */
    list_init(&queue);
    for (i = list_head(&active_filters); i; i = list_next(i))
    {
        cur = (filter *) list_data(i);
        if (hash_get(&valence, cur->name) == base)
            list_append(&queue, cur);
    }
    /* do a topological walk, starting at the back */
    while (list_head(&queue))
    {
        count--;
        cur = (filter *) list_data(list_head(&queue));
        list_erase(&queue, list_head(&queue), false);
        list_prepend(&active, cur);
        deps = (linked_list *) hash_get(&filter_dependencies, cur->name);
        if (deps)
        {
            for (j = list_head(deps); j; j = list_next(j))
            {
                name = (const char *) list_data(j);
                d = hash_get(&valence, name);
                if (d) /* otherwise a non-existant filter */
                {
                    d--;
                    hash_set(&valence, name, d);
                    if (d == base)
                        list_append(&queue, hash_get(&names, name));
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
    list_clear(&active_filters, false);
    memcpy(&active_filters, &active, sizeof(active));
    /* clean up */
    list_clear(&queue, false);
    hash_clear(&valence, false);
    hash_clear(&names, false);
}

void run_filters(function_call *call)
{
    list_node *i;
    filter *cur;
    void *data;

    if (dirty_active)
    {
        dirty_active = false;
        repair_filter_order();
    }

    call->generic.user_data = call_data;
    for (i = list_head(&active_filters); i != NULL; i = list_next(i))
    {
        cur = (filter *) list_data(i);
        if (cur->parent->offset >= 0)
            data = (void *)(((char *) call_data) + cur->parent->offset);
        else
            data = NULL;
        fprintf(stderr, "running filter %s on call %s\n",
                cur->name,
                function_table[call->generic.id].name);
        if (!(*cur->callback)(call, data)) break;
    }
}

filter_set *register_filter_set(const char *name,
                                filter_set_initialiser init,
                                filter_set_destructor done,
                                filter_set_set_variable set_variable)
{
    filter_set *s;

    s = (filter_set *) xmalloc(sizeof(filter_set));
    s->name = xstrdup(name);
    list_init(&s->filters);
    s->init = init;
    s->done = done;
    s->set_variable = set_variable;
    s->offset = (ptrdiff_t) -1;
    s->initialised = false;
    s->enabled = false;
    s->dl_handle = current_dl_handle;
    list_append(&filter_sets, s);
    return s;
}

void register_filter(filter_set *handle, const char *name,
                     filter_callback callback)
{
    filter *f;

    f = (filter *) xmalloc(sizeof(filter));
    f->name = xstrdup(name);
    f->callback = callback;
    f->parent = handle;
    list_append(&handle->filters, f);
}

void register_filter_depends(const char *after, const char *before)
{
    linked_list *deps;
    deps = (linked_list *) hash_get(&filter_dependencies, after);
    if (!deps)
    {
        deps = xmalloc(sizeof(linked_list));
        list_init(deps);
        hash_set(&filter_dependencies, after, deps);
    }
    list_append(deps, xstrdup(before));
}

void register_filter_set_depends(const char *base, const char *dep)
{
    list_append(&filter_set_dependencies[0], xstrdup(base));
    list_append(&filter_set_dependencies[1], xstrdup(dep));
}

void register_filter_set_call_state(filter_set *handle, size_t bytes)
{
    handle->offset = call_data_size;
    call_data_size += bytes;
    if (!call_data)
        call_data = xmalloc(call_data_size);
}

filter_set *get_filter_set_handle(const char *name)
{
    list_node *i;
    filter_set *cur;

    for (i = list_head(&filter_sets); i != NULL; i = list_next(i))
    {
        cur = (filter_set *) list_data(i);
        if (strcmp(name, cur->name) == 0)
            return cur;
    }
    fprintf(stderr, "Failed to locate filter-set %s\n", name);
    exit(1);
}

void *get_filter_set_symbol(filter_set *handle, const char *name)
{
    return dlsym(handle->dl_handle, name);
}
