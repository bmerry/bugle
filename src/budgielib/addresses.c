/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008, 2013  Bruce Merry
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
#include <stdio.h>
#include <bugle/memory.h>
#include <bugle/io.h>
#include <budgie/types.h>
#include <budgie/addresses.h>
#include <budgie/reflect.h>
#include "platform/threads.h"
#include "platform/dl.h"
#include "budgielib/lib.h"

/* External function provided to look up addresses on the fly, for when the
 * address is context-dependent.
 */
extern BUDGIEAPIPROC budgie_address_generator(budgie_function id);

static void make_indent(int indent, bugle_io_writer *writer)
{
    int i;
    for (i = 0; i < indent; i++)
        bugle_io_putc(' ', writer);
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

void budgie_call_parameter_dump(const generic_function_call *call, int param, bugle_io_writer *writer)
{
    const group_dump_parameter *info;
    int length = -1;
    const void *arg;

    info = parameter_info(call, param);
    length = budgie_call_parameter_length(call, param);
    arg = (param == -1) ? call->retn : call->args[param];

    if (!info->dumper || !info->dumper(call, param, arg, length, writer))
        budgie_dump_any_type(budgie_call_parameter_type(call, param),
                             arg, length, writer);
}

void budgie_dump_any_call(const generic_function_call *call, int indent, bugle_io_writer *writer)
{
    int i;

    make_indent(indent, writer);
    bugle_io_printf(writer, "%s(", budgie_function_name(call->id));
    for (i = 0; i < call->num_args; i++)
    {
        if (i) bugle_io_puts(", ", writer);
        budgie_call_parameter_dump(call, i, writer);
    }
    bugle_io_putc(')', writer);
    if (call->retn)
    {
        bugle_io_puts(" = ", writer);
        budgie_call_parameter_dump(call, -1, writer);
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
        id2 = budgie_function_next(id2);
    } while (id2 != id);
    return NULL;
}

BUDGIEAPIPROC budgie_function_address_wrapper(budgie_function id)
{
    assert(id >= 0 && id < budgie_function_count());
    return _budgie_function_address_wrapper[id];
}

void budgie_function_address_initialise(void)
{
    bugle_dl_module handle;
    int i;
    size_t N, F, j;

    N = _budgie_library_count;
    F = budgie_function_count();

    bugle_dl_init();
    /* If we search EGL for GLES symbols, the ARM GLES2 emulator will
     * return symbols from the underlying GL driver. Reversing the library
     * order is a hack (ideally we should search each library only for the
     * appropriate symbols).
     */
    for (i = N - 1; i >= 0; i--)
    {
        handle = bugle_dl_open(_budgie_library_names[i], 0);
        if (handle)
        {
            for (j = 0; j < F; j++)
                if (!_budgie_function_address_real[j])
                {
                    _budgie_function_address_real[j] =
                        bugle_dl_sym_function(handle, budgie_function_name(j));
                }
        }
        else
        {
            fprintf(stderr, "Failed to open %s", _budgie_library_names[i]);
            exit(1);
        }
    }
}

void budgie_function_address_set_real(budgie_function id, BUDGIEAPIPROC addr)
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
    bugle_thread_key_create(&reentrance_key, NULL);
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
