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
#define _XOPEN_SOURCE
#include "src/filters.h"
#include "src/utils.h"
#include "src/types.h"
#include "src/canon.h"
#include "src/hashtable.h"
#include "src/glutils.h"
#include "budgielib/state.h"
#include "common/protocol.h"
#include "common/bool.h"
#include "common/safemem.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static int in_pipe, out_pipe;
static bool break_on[NUMBER_OF_FUNCTIONS];
static bool break_on_error;

#if 0
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
#endif

static void debugger_loop(function_call *call, void *data)
{
    uint32_t req, req_val;
    char *req_str, *resp_str;
    budgie_function func;

    /* FIXME: error checking */
    if (begin_internal_render())
    {
        CALL_glFinish();
        end_internal_render("debugger", true);
    }
    while (true)
    {
        recv_code(in_pipe, &req);
        if (req == REQ_CONT) break;
        switch (req)
        {
        case REQ_BREAK:
            recv_string(in_pipe, &req_str);
            recv_code(in_pipe, &req_val);
            func = find_function(req_str);
            if (func != NULL_FUNCTION)
            {
                send_code(out_pipe, RESP_ANS);
                send_code(out_pipe, 0);
                break_on[func] = req_val != 0;
            }
            else
            {
                send_code(out_pipe, RESP_ERROR);
                send_code(out_pipe, 0);
                xasprintf(&resp_str, "Unknown function %s", req_str);
                send_string(out_pipe, resp_str);
                free(resp_str);
            }
            free(req_str);
            break;
        case REQ_BREAK_ERROR:
            recv_code(in_pipe, &req_val);
            break_on_error = req_val != 0;
            send_code(out_pipe, RESP_ANS);
            send_code(out_pipe, 0);
            break;
        case REQ_STATE:
            recv_string(in_pipe, &req_str);
            send_code(out_pipe, RESP_ERROR);
            send_code(out_pipe, 0);
            send_string(out_pipe, "REQ_STATE not supported yet");
            break;
        case REQ_QUIT:
            exit(1);
        }
    }
}

static bool debugger_callback(function_call *call, void *data)
{
    if (break_on[canonical_call(call)])
    {
        send_code(out_pipe, RESP_BREAK);
        send_string(out_pipe, function_table[call->generic.id].name);
        debugger_loop(call, data);
    }
    return true;
}

static bool debugger_error_callback(function_call *call, void *data)
{
    GLenum error;
    if (break_on_error
        && (error = get_call_error(call)))
    {
        send_code(out_pipe, RESP_BREAK_ERROR);
        send_string(out_pipe, function_table[call->generic.id].name);
        send_string(out_pipe, gl_enum_to_token(error));
        debugger_loop(call, data);
    }
    return true;
}

static bool initialise_debugger(filter_set *handle)
{
    register_filter(handle, "debugger", debugger_callback);
    register_filter(handle, "debugger_error", debugger_error_callback);
    register_filter_depends("invoke", "debugger");
    register_filter_depends("debugger_error", "invoke");
    register_filter_depends("debugger_error", "error");
    filter_set_renders("debugger");
    filter_post_renders("debugger_error");
    filter_set_queries_error("debugger", false);

    /* FIXME: validate the numbers and the function calls */
    if (getenv("BUGLE_DEBUGGER_FD_IN"))
        in_pipe = atoi(getenv("BUGLE_DEBUGGER_FD_IN"));
    else
        in_pipe = 0;
    if (getenv("BUGLE_DEBUGGER_FD_OUT"))
        out_pipe = atoi(getenv("BUGLE_DEBUGGER_FD_OUT"));
    else
        out_pipe = 1;
    return true;
}

static bool set_variable_debugger(filter_set *handle,
                                  const char *name,
                                  const char *value)
{
    budgie_function f;

    if (strcmp(name, "break") == 0)
    {
        if (strcmp(value, "error") == 0)
            break_on_error = true;
        else
        {
            f = find_function(value);
            if (f == NULL_FUNCTION)
                fprintf(stderr, "Warning: unknown function %s\n", value);
            else
                break_on[canonical_function(f)] = true;
        }
        return true;
    }
    return false;
}

void initialise_filter_library(void)
{
    memset(break_on, 0, sizeof(break_on));
    register_filter_set("debugger", initialise_debugger, NULL, set_variable_debugger);
}
