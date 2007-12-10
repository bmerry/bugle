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
#define _POSIX_SOURCE
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <ltdl.h>
#include "budgieutils.h"
#include "typeutils.h"
#include "ioutils.h"
#include <stdbool.h>
#include "common/threads.h"
#include "common/safemem.h"
#include "xalloc.h"

void budgie_dump_any_call(const generic_function_call *call, int indent, FILE *out)
{
    size_t i;
    const group_data *data;
    arg_dumper cur_dumper;
    budgie_type type;
    int length;
    const group_parameter_data *info;

    budgie_make_indent(indent, out);
    fprintf(out, "%s(", budgie_function_table[call->id].name);
    data = &budgie_group_table[call->group];
    info = &data->parameters[0];
    for (i = 0; i < data->num_parameters; i++, info++)
    {
        if (i) fputs(", ", out);
        cur_dumper = info->dumper; /* custom dumper */
        length = -1;
        if (info->get_length) length = (*info->get_length)(call, i, call->args[i]);
        if (!cur_dumper || !(*cur_dumper)(call, i, call->args[i], length, out))
        {
            type = info->type;
            if (info->get_type) type = (*info->get_type)(call, i, call->args[i]);
            budgie_dump_any_type(type, call->args[i], length, out);
        }
    }
    fputs(")", out);
    if (call->retn)
    {
        fputs(" = ", out);
        info = &data->retn;
        cur_dumper = info->dumper; /* custom dumper */
        length = -1;
        if (info->get_length) length = (*info->get_length)(call, i, call->retn);
        if (!cur_dumper || !(*cur_dumper)(call, -1, call->retn, length, out))
        {
            type = info->type;
            if (info->get_type) type = (*info->get_type)(call, -1, call->retn);
            budgie_dump_any_type(type, call->retn, length, out);
        }
    }
}

void (*budgie_get_function_wrapper(const char *name))(void)
{
    int l, r, m;

    l = 0;
    r = budgie_number_of_functions;
    while (r - l > 1)
    {
        m = (l + r) / 2;
        if (strcmp(name, budgie_function_name_table[m].name) >= 0)
            l = m;
        else
            r = m;
    }
    if (strcmp(name, budgie_function_name_table[l].name) == 0)
        return budgie_function_name_table[l].wrapper;
    else
        return (void (*)(void)) NULL;
}

void initialise_real(void)
{
    lt_dlhandle handle;
    size_t i, j;
    size_t N, F;

    N = budgie_number_of_libraries;
    F = budgie_number_of_functions;

    lt_dlmalloc = xmalloc;
    lt_dlrealloc = xrealloc;
    lt_dlinit();
    for (i = 0; i < N; i++)
    {
        handle = lt_dlopen(library_names[i]);
        if (handle)
        {
            for (j = 0; j < F; j++)
                if (!budgie_function_table[j].real)
                {
                    budgie_function_table[j].real = (void (*)(void)) lt_dlsym(handle, budgie_function_table[j].name);
                    lt_dlerror(); /* clear the error flag */
                }
        }
        else
        {
            fprintf(stderr, "%s", lt_dlerror());
            exit(1);
        }
    }
}

/* Re-entrance protection. Note that we still wish to allow other threads
 * to enter from the user application; it is just that we do not want the
 * real library to call our fake functions.
 *
 * Also note that the protection is called *before* we first encounter
 * the interceptor, so we cannot rely on the interceptor to initialise us.
 */

static bugle_thread_key_t reentrance_key;

BUGLE_CONSTRUCTOR(initialise_reentrance);
static void initialise_reentrance(void)
{
    bugle_thread_key_create(&reentrance_key, NULL);
}

/* Sets the flag to mark entry, and returns true if we should call
 * the interceptor. We set an arbitrary non-NULL pointer.
 */
bool check_set_reentrance(void)
{
    /* Note that since the data is thread-specific, we need not worry
     * about contention.
     */
    bool ans;

    BUGLE_RUN_CONSTRUCTOR(initialise_reentrance);
    ans = bugle_thread_getspecific(reentrance_key) == NULL;
    bugle_thread_setspecific(reentrance_key, &ans); /* arbitrary non-NULL value */
    return ans;
}

void clear_reentrance(void)
{
    bugle_thread_setspecific(reentrance_key, NULL);
}
