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
#define _POSIX_SOURCE
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include "common/bool.h"
#include "budgieutils.h"
#include <setjmp.h>
#include <dlfcn.h>

void dump_bitfield(unsigned int value, FILE *out,
                   bitfield_pair *tags, int count)
{
    bool first = true;
    int i;
    for (i = 0; i < count; i++)
        if (value & tags[i].value)
        {
            if (!first) fputs(" | ", out); else first = false;
            fputs(tags[i].name, out);
            value &= ~tags[i].value;
        }
    if (value)
    {
        if (!first) fputs(" | ", out);
        fprintf(out, "%08x", value);
    }
}

bool dump_string(const void *value, int count, FILE *out)
{
    /* FIXME: handle illegal dereferences */
    const char *str = *(const char * const *) value;
    if (str == NULL) fputs("NULL", out);
    else fprintf(out, "\"%s\"", str);
    return true;
}

int count_string(const void *value)
{
    /* FIXME: handle illegal deferences */
    const char *str = (const char *) value;
    if (str == NULL) return 0;
    else return strlen(str) + 1;
}

void dump_any_type(budgie_type type, const void *value, int length, FILE *out)
{
    const type_data *info;
    budgie_type new_type;

    assert(type >= 0);
    info = &type_table[type];
    if (info->get_type) /* highly unlikely */
    {
        new_type = (*info->get_type)(value);
        if (new_type != type)
        {
            dump_any_type(new_type, value, length, out);
            return;
        }
    }
    /* More likely e.g. for strings. Note that we don't override the length
     * if specified, since it may come from a parameter override
     */
    if (info->get_length && length == -1) 
        length = (*info->get_length)(value);

    if (!info->custom_dumper
        || !(*info->custom_dumper)(value, length, out))
    {
        assert(info->dumper);
        (*info->dumper)(value, length, out);
    }
}

void dump_any_call(const generic_function_call *call, int indent, FILE *out)
{
    size_t i;
    const function_data *data;
    arg_dumper cur_dumper;
    budgie_type type;
    int length;
    const function_parameter_data *info;

    make_indent(indent, out);
    data = &function_table[call->id];
    fputs(data->name, out);
    fputs("(", out);
    info = &data->parameters[0];
    for (i = 0; i < data->num_parameters; i++, info++)
    {
        if (i) fputs(", ", out);
        cur_dumper = info->dumper; /* custom dumper */
        if (!cur_dumper || !(*cur_dumper)(call, i, call->args[i], out))
        {
            type = info->type;
            length = -1;
            if (info->get_type) type = (*info->get_type)(call, i, call->args[i]);
            if (info->get_length) length = (*info->get_length)(call, i, call->args[i]);
            dump_any_type(type, call->args[i], length, out);
        }
    }
    fputs(")", out);
    if (call->retn)
    {
        fputs(" = ", out);
        info = &data->retn;
        cur_dumper = info->dumper; /* custom dumper */
        if (!cur_dumper || !(*cur_dumper)(call, -1, call->retn, out))
        {
            type = info->type;
            length = -1;
            if (info->get_type) type = (*info->get_type)(call, -1, call->retn);
            if (info->get_length) length = (*info->get_length)(call, i, call->retn);
            dump_any_type(type, call->retn, length, out);
        }
    }
    fputs("\n", out);
}

void make_indent(int indent, FILE *out)
{
    int i;
    for (i = 0; i < indent; i++)
        fputc(' ', out);
}

void initialise_real(void)
{
    void *handle;
    size_t i, j;
    int N, F;
    static bool done = false;

    /* We have to provide this protection, because the interceptor function
     * call initialise_real if they are re-entered so that they can bypass
     * the filter chain.
     */
    if (done) return;
    done = true;

    N = number_of_libraries;
    F = number_of_functions;
    for (i = 0; i < N; i++)
    {
        handle = dlopen(library_names[i], RTLD_LAZY);
        if (handle)
        {
            for (j = 0; j < F; j++)
                if (!function_table[j].real)
                {
                    function_table[j].real = (void (*)(void)) dlsym(handle, function_table[j].name);
                    dlerror(); /* clear the error flag */
                }
        }
        else
        {
            fprintf(stderr, "%s", dlerror());
            exit(1);
        }
    }
}

/* Memory validation */

static sigjmp_buf valid_buf;

static void valid_sigsegv_handler(int sig)
{
    siglongjmp(valid_buf, 1);
}

bool valid_address_range(const char *base, size_t count)
{
    volatile char test;
    struct sigaction old, act;
    bool result = false;

    if (sigsetjmp(valid_buf, 1) == 0)
    {
        act.sa_handler = valid_sigsegv_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        while (sigaction(SIGSEGV, &act, &old) == -1)
        {
            if (errno != EINTR)
            {
                perror("failed to set SIGSEGV handler");
                exit(1);
            }
        }
        while (count)
        {
            test = *base;
            base++;
            count--;
        }
        result = true;
    }
    while (sigaction(SIGSEGV, &old, NULL) == -1)
    {
        if (errno != EINTR)
        {
            perror("failed to reset SIGSEGV handler");
            exit(1);
        }
    }
    return result;
}