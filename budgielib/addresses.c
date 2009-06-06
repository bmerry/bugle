/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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
#include <bugle/memory.h>
#include <budgie/types.h>
#include <budgie/addresses.h>
#include <budgie/reflect.h>
#include "threads.h"
#include "budgielib/lib.h"

/* External function provided to look up addresses on the fly, for when the
 * address is context-dependent.
 */
extern BUDGIEAPIPROC budgie_address_generator(budgie_function id);

static void make_indent(int indent, char **buffer, size_t *size)
{
    int i;
    for (i = 0; i < indent; i++)
        budgie_snputc_advance(buffer, size, ' ');
}

static const group_dump_parameter *parameter_info(const generic_function_call *call, int param)
{
    assert(param >= -1 && param < call->num_args);
    if (param == -1)
    {
        assert(call->retn);
        return &_budgie_group_dump_table[call->group].retn;
    }
    else
        return &_budgie_group_dump_table[call->group].parameters[param];
}

budgie_type budgie_call_parameter_type(const generic_function_call *call, int param)
{
    const group_dump_parameter *info;
    const void *arg;

    info = parameter_info(call, param);
    arg = (param == -1) ? call->retn : call->args[param];
    if (info->get_type)
        return info->get_type(call, param, arg);
    else
        return budgie_type_type(budgie_group_parameter_type(call->group, param), 
                                arg);
}

int budgie_call_parameter_length(const generic_function_call *call, int param)
{
    const group_dump_parameter *info;
    const void *arg;

    info = parameter_info(call, param);
    arg = (param == -1) ? call->retn : call->args[param];
    if (info->get_length)
        return info->get_length(call, param, arg);
    else
        return budgie_type_length(budgie_group_parameter_type(call->group, param), 
                                  arg);
}

void budgie_call_parameter_dump(const generic_function_call *call, int param, char **buffer, size_t *size)
{
    const group_dump_parameter *info;
    int length = -1;
    const void *arg;

    info = parameter_info(call, param);
    length = budgie_call_parameter_length(call, param);
    arg = (param == -1) ? call->retn : call->args[param];

    if (!info->dumper || !info->dumper(call, param, arg, length, buffer, size))
        budgie_dump_any_type(budgie_call_parameter_type(call, param),
                             arg, length, buffer, size);
}

void budgie_dump_any_call(const generic_function_call *call, int indent, char **buffer, size_t *size)
{
    int i;

    make_indent(indent, buffer, size);
    budgie_snprintf_advance(buffer, size, "%s(", budgie_function_name(call->id));
    for (i = 0; i < call->num_args; i++)
    {
        if (i) budgie_snputs_advance(buffer, size, ", ");
        budgie_call_parameter_dump(call, i, buffer, size);
    }
    budgie_snputc_advance(buffer, size, ')');
    if (call->retn)
    {
        budgie_snputs_advance(buffer, size, " = ");
        budgie_call_parameter_dump(call, -1, buffer, size);
    }
}

static BUDGIEAPIPROC function_address_real1(budgie_function id)
{
    assert(id >= 0 && id < budgie_function_count());
    if (_budgie_function_address_real[id] == NULL)
        return budgie_address_generator(id);
    else
        return _budgie_function_address_real[id];
}

BUDGIEAPIPROC budgie_function_address_real(budgie_function id)
{
    BUDGIEAPIPROC fn;
    budgie_function id2;

    assert(id >= 0 && id < budgie_function_count());
    id2 = id;
    do
    {
        fn = function_address_real1(id2);
        if (fn != NULL)
            return fn;
        id2 = budgie_function_next(id);
    } while (id2 != id);
    return NULL;
}

void (BUDGIEAPI *budgie_function_address_wrapper(budgie_function id))(void)
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

    /* These have gone away at some point in libtool
     */
#if 0
    lt_dlmalloc = bugle_malloc;
    lt_dlrealloc = xrealloc;
#endif
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
                        (void (BUDGIEAPI *)(void)) lt_dlsym(handle, budgie_function_name(j));
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

void budgie_function_address_set_real(budgie_function id, void (BUDGIEAPI *addr)(void))
{
    assert(id >= 0 && id < budgie_function_count());
    _budgie_function_address_real[id] = addr;
}

void budgie_function_set_bypass(budgie_function id, bugle_bool bypass)
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

static bugle_thread_key_t reentrance_key;

BUGLE_CONSTRUCTOR(reentrance_initialise);
static void reentrance_initialise(void)
{
    bugle_thread_key_create(reentrance_key, NULL);
}

/* Sets the flag to mark entry, and returns BUGLE_TRUE if we should call
 * the interceptor. We set an arbitrary non-NULL pointer.
 */
bugle_bool _budgie_reentrance_init(void)
{
    /* Note that since the data is thread-specific, we need not worry
     * about contention.
     */
    bugle_bool ans;

    BUGLE_RUN_CONSTRUCTOR(reentrance_initialise);
    ans = bugle_thread_getspecific(reentrance_key) == NULL;
    bugle_thread_setspecific(reentrance_key, &ans); /* arbitrary non-NULL value */
    return ans;
}

void _budgie_reentrance_clear(void)
{
    bugle_thread_setspecific(reentrance_key, NULL);
}
