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
#include "src/filters.h"
#include "src/utils.h"
#include "src/types.h"
#include "src/canon.h"
#include "src/hashtable.h"
#include "budgielib/state.h"
#include "common/bool.h"
#include <stdio.h>
#include <string.h>

static FILE *in_pipe, *out_pipe;
bool break_on[NUMBER_OF_FUNCTIONS];

static void dump_state(state_generic *state, int indent)
{
    bool big = false;
    int i;
    state_generic **children;

    make_indent(indent, out_pipe);
    fputs(state->spec->name, out_pipe);
    if (state->data)
    {
        fputs(" = ", out_pipe);
        dump_any_type(state->spec->data_type, get_state_current(state), 1, out_pipe);
    }
    fputs("\n", out_pipe);
    if (state->num_indexed)
    {
        big = true;
        make_indent(indent, out_pipe);
        fputs("{\n", out_pipe);
        for (i = 0; i < state->num_indexed; i++)
            dump_state(state->indexed[i], indent + 4);
    }
    for (children = state->children; *children; children++)
    {
        if (!big)
        {
            big = true;
            make_indent(indent, out_pipe);
            fputs("{\n", out_pipe);
        }
        dump_state(*children, indent + 4);
    }
    if (big)
    {
        make_indent(indent, out_pipe);
        fputs("}\n", out_pipe);
    }
}

static bool debugger_callback(function_call *call, void *data)
{
    char line[1024];
    if (break_on[canonical_call(call)])
    {
        (*glFinish_real)();
        fgets(line, sizeof(line), in_pipe);
        if (strcmp(line, "state\n") == 0)
        {
            fputs("dumping state\n", out_pipe);
            dump_state(&get_root_state()->generic, 0);
        }
        else if (strcmp(line, "quit\n") == 0)
            exit(0);
    }
    return true;
}

static bool initialise_debugger(filter_set *handle)
{
    register_filter(handle, "debugger", debugger_callback);
    register_filter_depends("invoke", "debugger");
    register_filter_set_depends("debugger", "trackcontext");
    in_pipe = stdin;
    out_pipe = stdout;
    return true;
}

static bool set_variable_debugger(filter_set *handle,
                                  const char *name,
                                  const char *value)
{
    budgie_function f;

    if (strcmp(name, "break") == 0)
    {
        f = find_function(value);
        if (f == NULL_FUNCTION)
            fprintf(stderr, "Warning: unknown function %s", value);
        else
            break_on[canonical_function(f)] = true;
        return true;
    }
    return false;
}

void initialise_filter_library(void)
{
    memset(break_on, 0, sizeof(break_on));
    register_filter_set("debugger", initialise_debugger, NULL, set_variable_debugger);
}
