#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _GNU_SOURCE
#include "src/filters.h"
#include "src/types.h"
#include "src/utils.h"
#include "src/linkedlist.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

static linked_list filter_sets;
static linked_list active_filters;
static linked_list filter_dependencies[2];
static linked_list filter_set_dependencies[2];
static bool dirty_active = false;
static void *call_data = NULL;
static size_t call_data_size = 0;

static void *current_dl_handle = NULL;

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
    list_init(&filter_dependencies[0]);
    list_init(&filter_dependencies[1]);
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
        full_name = (char *) malloc(strlen(PKGLIBDIR) + strlen(ent->d_name) + 2);
        sprintf(full_name, PKGLIBDIR "/%s", ent->d_name);
        handle = dlopen(full_name, RTLD_LAZY);
        if (handle == NULL) continue;
        init = dlsym(handle, "initialise_filter_library");
        if (init == NULL) continue;
        current_dl_handle = handle;
        (*init)();
        current_dl_handle = NULL;
        free(full_name);
    }
}

void enable_filter_set(filter_set *handle)
{
    list_node *i, *j;

    if (!handle->enabled)
    {
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

void run_filters(function_call *call)
{
    list_node *i, *j, *k;
    filter *cur, *nxt;
    bool more;
    int loops = 0, count;
    void *data;

    if (dirty_active)
    {
        dirty_active = false;
        /* FIXME: check for infinite loop */
        do
        {
            loops++;
            count = 1;
            more = false;
            for (i = list_head(&active_filters); list_next(i) != NULL; i = list_next(i))
            {
                cur = (filter *) list_data(i);
                nxt = (filter *) list_data(list_next(i));
                for (j = list_head(&filter_dependencies[0]),
                     k = list_head(&filter_dependencies[1]);
                     j != NULL;
                     j = list_next(j), k = list_next(k))
                {
                    if (strcmp((char *) list_data(j), cur->name) == 0
                        && strcmp((char *) list_data(k), nxt->name) == 0) break;
                }
                if (j)
                {
                    list_set_data(i, nxt);
                    list_set_data(list_next(i), cur);
                    more = true;
                }
                count++;
            }
        } while (more && loops <= count);
        if (more)
        {
            fprintf(stderr, "Cyclic dependency between filters");
            exit(1);
        }
    }

    call->generic.user_data = call_data;
    for (i = list_head(&active_filters); i != NULL; i = list_next(i))
    {
        cur = (filter *) list_data(i);
        if (cur->parent->offset >= 0)
            data = (void *)(((char *) call_data) + cur->parent->offset);
        else
            data = NULL;
        if (!(*cur->callback)(call, data)) break;
    }
}

filter_set *register_filter_set(const char *name,
                                filter_set_initialiser init,
                                filter_set_destructor done)
{
    filter_set *s;

    s = (filter_set *) malloc(sizeof(filter_set));
    s->name = strdup(name);
    list_init(&s->filters);
    s->init = init;
    s->done = done;
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

    f = (filter *) malloc(sizeof(filter));
    f->name = strdup(name);
    f->callback = callback;
    f->parent = handle;
    list_append(&handle->filters, f);
}

void register_filter_depends(const char *after, const char *before)
{
    list_append(&filter_dependencies[0], strdup(after));
    list_append(&filter_dependencies[1], strdup(before));
}

void register_filter_set_depends(const char *base, const char *dep)
{
    list_append(&filter_set_dependencies[0], strdup(base));
    list_append(&filter_set_dependencies[1], strdup(dep));
}

void register_filter_set_call_state(filter_set *handle, size_t bytes)
{
    handle->offset = call_data_size;
    call_data_size += bytes;
    if (!call_data)
        call_data = malloc(call_data_size);
}

filter_set *get_filter_set_handle(const char *name)
{
    list_node *i;
    filter_set *cur;

    for (i = list_head(&filter_sets); i != NULL; i = list_next(i))
    {
        cur = (filter_set *) list_data(i);
        if (strcmp(name, cur->name) == 0)
        {
            if (!cur->initialised)
            {
                if (!(*cur->init)(cur))
                {
                    fprintf(stderr, "Failed to initialise filter-set %s\n", name);
                    exit(1);
                }
                cur->initialised = true;
            }
            return cur;
        }
    }
    fprintf(stderr, "Failed to locate filter-set %s\n", name);
    exit(1);
}

void *get_filter_set_symbol(filter_set *handle, const char *name)
{
    return dlsym(handle->dl_handle, name);
}
