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
#include <stddef.h>
#include <assert.h>
#include <ltdl.h>
#include <stdio.h>
#include <budgie/types.h>
#include <budgie/addresses.h>
#include <budgie/reflect.h>
#include "common/threads.h"
#include "budgielib/lib.h"
#include "tls.h"
#include "xalloc.h"

static void make_indent(int indent, FILE *out)
{
    int i;
    for (i = 0; i < indent; i++)
        fputc(' ', out);
}

void budgie_dump_any_call(const generic_function_call *call, int indent, FILE *out)
{
    size_t i;
    const group_dump *data;
    arg_dumper cur_dumper;
    budgie_type type;
    int length;
    const group_dump_parameter *info;

    make_indent(indent, out);
    fprintf(out, "%s(", budgie_function_name(call->id));
    data = &_budgie_group_dump_table[call->group];
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

void (*budgie_function_address_real(budgie_function id))(void)
{
    assert(id >= 0 && id < budgie_function_count());
    return _budgie_function_address_real[id];
}

void (*budgie_function_address_wrapper(budgie_function id))(void)
{
    assert(id >= 0 && id < budgie_function_count());
    return _budgie_function_address_wrapper[id];
}

void budgie_function_address_initialise(void)
{
    lt_dlhandle handle;
    size_t i, j;
    size_t N, F;

    N = _budgie_library_count;
    F = budgie_function_count();

    lt_dlmalloc = xmalloc;
    lt_dlrealloc = xrealloc;
    lt_dlinit();
    for (i = 0; i < N; i++)
    {
        handle = lt_dlopen(_budgie_library_names[i]);
        if (handle)
        {
            for (j = 0; j < F; j++)
                if (!_budgie_function_address_real[j])
                {
                    _budgie_function_address_real[j] =
                        (void (*)(void)) lt_dlsym(handle, budgie_function_name(j));
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

void budgie_function_address_set_real(budgie_function id, void (*addr)(void))
{
    assert(id >= 0 && id < budgie_function_count());
    _budgie_function_address_real[id] = addr;
}

void budgie_function_set_bypass(budgie_function id, bool bypass)
{
    assert(id >= 0 && id < budgie_function_count());
    _budgie_bypass[id] = bypass;
}

/* Re-entrance protection. Note that we still wish to allow other threads
 * to enter from the user application; it is just that we do not want the
 * real library to call our fake functions.
 *
 * Also note that the protection is called *before* we first encounter
 * the interceptor, so we cannot rely on the interceptor to initialise us.
 *
 * The declaraion for these methods is in lib.h, but they're implemented
 * here to avoid quoting them for generation in lib.c.
 */

static gl_tls_key_t reentrance_key;

BUGLE_CONSTRUCTOR(reentrance_initialise);
static void reentrance_initialise(void)
{
    gl_tls_key_init(reentrance_key, NULL);
}

/* Sets the flag to mark entry, and returns true if we should call
 * the interceptor. We set an arbitrary non-NULL pointer.
 */
bool _budgie_reentrance_init(void)
{
    /* Note that since the data is thread-specific, we need not worry
     * about contention.
     */
    bool ans;

    BUGLE_RUN_CONSTRUCTOR(reentrance_initialise);
    ans = gl_tls_get(reentrance_key) == NULL;
    gl_tls_set(reentrance_key, &ans); /* arbitrary non-NULL value */
    return ans;
}

void _budgie_reentrance_clear(void)
{
    gl_tls_set(reentrance_key, NULL);
}
