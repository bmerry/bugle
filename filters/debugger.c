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
#include "src/canon.h"
#include "src/glutils.h"
#include "src/tracker.h"
#include "budgielib/state.h"
#include "common/hashtable.h"
#include "common/protocol.h"
#include "common/bool.h"
#include "common/safemem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

static int in_pipe = -1, out_pipe = -1;
static bool break_on[NUMBER_OF_FUNCTIONS];
static bool break_on_error = true, break_on_next = false;

static void dump_state(state_generic *state, int indent, FILE *out)
{
    bool big = false;
    int i;
    state_generic **children;
    char *ptr;

    budgie_make_indent(indent, out);
    fputs(state->name, out);
    if (state->data)
    {
        fputs(" = ", out);
        if (state->spec->data_length > 1)
        {
            fputs("(", out);
            ptr = budgie_get_state_current(state);
            for (i = 0; i < state->spec->data_length; i++)
            {
                if (i) fputs(", ", out);
                budgie_dump_any_type(state->spec->data_type, ptr,
                                     -1, out);
                ptr += budgie_type_table[state->spec->data_type].size;
            }
            fputs(")", out);
        }
        else
            budgie_dump_any_type(state->spec->data_type, budgie_get_state_current(state),
                                 -1, out);
    }
    fputs("\n", out);
    if (state->num_indexed)
    {
        big = true;
        budgie_make_indent(indent, out);
        fputs("{\n", out);
        for (i = 0; i < state->num_indexed; i++)
            dump_state(state->indexed[i], indent + 4, out);
    }
    for (children = state->children; *children; children++)
    {
        if (!big)
        {
            big = true;
            budgie_make_indent(indent, out);
            fputs("{\n", out);
        }
        dump_state(*children, indent + 4, out);
    }
    if (big)
    {
        budgie_make_indent(indent, out);
        fputs("}\n", out);
    }
}

/* A driver for budgie_string_io, for state dumping. */
static void dump_string_state(FILE *f, void *data)
{
    dump_state((state_generic *) data, 0, f);
}

static void dump_ppm_header(FILE *f, void *data)
{
    int *wh;

    wh = (int *) data;
    fprintf(f, "P6\n%d %d\n255\n", wh[0], wh[1]);
}

static bool debugger_screenshot(int pipe)
{
    Display *dpy;
    GLXContext aux, real;
    GLXDrawable old_read, old_write;
    int width, height, stride;
    int wh[2];
    size_t header_len;
    char *header;
    char *data, *in, *out;
    int i;

    aux = bugle_get_aux_context();
    if (!aux || !bugle_begin_internal_render()) return false;
    real = CALL_glXGetCurrentContext();
    old_write = CALL_glXGetCurrentDrawable();
    old_read = CALL_glXGetCurrentReadDrawable();
    dpy = CALL_glXGetCurrentDisplay();
    CALL_glXQueryDrawable(dpy, old_write, GLX_WIDTH, &width);
    CALL_glXQueryDrawable(dpy, old_write, GLX_HEIGHT, &height);
    CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);

    wh[0] = width;
    wh[1] = height;
    stride = ((3 * width + 3) & ~3);
    header = budgie_string_io(dump_ppm_header, wh);
    data = bugle_malloc(stride * height);
    CALL_glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);

    header_len = strlen(header);
    header = bugle_realloc(header, header_len + width * height * 3);
    in = data + (height - 1) * stride;
    out = header + header_len;
    for (i = 0; i < height; i++)
    {
        memcpy(out, in, width * 3);
        out += width * 3;
        in -= stride;
    }

    gldb_send_code(pipe, RESP_SCREENSHOT);
    gldb_send_binary_string(pipe, header_len + width * height * 3, header);
    free(header);
    free(data);

    CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
    bugle_end_internal_render("debugger_screenshot", true);
    return true;
}

/* The main initialiser calls this with init = true. It has essentially
 * the usual effects, but refuses to do anything that doesn't make sense
 * until the program is properly running (such as flush or query state).
 */
static void debugger_loop(bool init)
{
    uint32_t req, req_val;
    char *req_str, *resp_str;
    budgie_function func;
    state_generic *state;
    state_7context_I *ctx;
    filter_set *f;
    bool enable;

    /* FIXME: error checking on the network code */
    if (!init && bugle_begin_internal_render())
    {
        CALL_glFinish();
        bugle_end_internal_render("debugger", true);
    }
    while (true)
    {
        gldb_recv_code(in_pipe, &req);
        if (req == REQ_CONT || req == REQ_STEP || req == REQ_RUN)
        {
            if (req == REQ_RUN)
                gldb_send_code(out_pipe, RESP_RUNNING);
            break_on_next = (req == REQ_STEP);
            break;
        }
        switch (req)
        {
        case REQ_BREAK:
            gldb_recv_string(in_pipe, &req_str);
            gldb_recv_code(in_pipe, &req_val);
            func = bugle_find_function(req_str);
            if (func != NULL_FUNCTION)
            {
                gldb_send_code(out_pipe, RESP_ANS);
                gldb_send_code(out_pipe, 0);
                break_on[func] = req_val != 0;
            }
            else
            {
                gldb_send_code(out_pipe, RESP_ERROR);
                gldb_send_code(out_pipe, 0);
                bugle_asprintf(&resp_str, "Unknown function %s", req_str);
                gldb_send_string(out_pipe, resp_str);
                free(resp_str);
            }
            free(req_str);
            break;
        case REQ_BREAK_ERROR:
            gldb_recv_code(in_pipe, &req_val);
            break_on_error = req_val != 0;
            gldb_send_code(out_pipe, RESP_ANS);
            gldb_send_code(out_pipe, 0);
            break;
        case REQ_ENABLE_FILTERSET:
        case REQ_DISABLE_FILTERSET:
            enable = (req == REQ_ENABLE_FILTERSET);
            gldb_recv_string(in_pipe, &req_str);
            f = bugle_get_filter_set_handle(req_str);
            if (!f)
            {
                bugle_asprintf(&resp_str, "Unknown filter-set %s", req_str);
                gldb_send_code(out_pipe, RESP_ERROR);
                gldb_send_code(out_pipe, 0);
                gldb_send_string(out_pipe, resp_str);
                free(resp_str);
            }
            else if (bugle_filter_set_is_enabled(f) == enable)
            {
                bugle_asprintf(&resp_str, "Filter-set %s is already %s",
                          req_str, enable ? "enabled" : "disabled");
                gldb_send_code(out_pipe, RESP_ERROR);
                gldb_send_code(out_pipe, 0);
                gldb_send_string(out_pipe, resp_str);
                free(resp_str);
            }
            else
            {
                if (enable) bugle_enable_filter_set(f);
                else bugle_disable_filter_set(f);
                if (!bugle_filter_set_is_enabled(bugle_get_filter_set_handle("debugger")))
                {
                    gldb_send_code(out_pipe, RESP_ERROR);
                    gldb_send_code(out_pipe, 0);
                    gldb_send_string(out_pipe, "Debugger was disabled; re-enabling");
                    bugle_enable_filter_set(bugle_get_filter_set_handle("debugger"));
                }
                else
                {
                    gldb_send_code(out_pipe, RESP_ANS);
                    gldb_send_code(out_pipe, 0);
                }
            }
            free(req_str);
            break;
        case REQ_STATE:
            gldb_recv_string(in_pipe, &req_str);
            ctx = bugle_tracker_get_context_state();
            if (!ctx)
            {
                gldb_send_code(out_pipe, RESP_ERROR);
                gldb_send_code(out_pipe, 0);
                gldb_send_string(out_pipe, "No context");
                break;
            }
            if (*req_str)
            {
                state = budgie_get_state_by_name(&ctx->generic, req_str);
                if (!state)
                {
                    gldb_send_code(out_pipe, RESP_ERROR);
                    gldb_send_code(out_pipe, 0);
                    gldb_send_string(out_pipe, "No such state");
                    break;
                }
                resp_str = budgie_string_io(dump_string_state, state);
            }
            else
                resp_str = budgie_string_io(dump_string_state, &ctx->generic);
            gldb_send_code(out_pipe, RESP_STATE);
            gldb_send_string(out_pipe, resp_str);
            free(resp_str);
            break;
        case REQ_SCREENSHOT:
            if (!debugger_screenshot(out_pipe))
            {
                gldb_send_code(out_pipe, RESP_ERROR);
                gldb_send_code(out_pipe, 0);
                gldb_send_string(out_pipe, "Not able to call GL at this time");
            }
            break;
        case REQ_QUIT:
            exit(1);
        case REQ_ASYNC:
            /* Ignore this, since we are stopped anyway */
            break;
        }
    }
}

/* Check for asynchronous requests to stop */
static void check_async(void)
{
    fd_set readfds;
    struct timeval timeout;
    int ret;

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(in_pipe, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        ret = select(in_pipe + 1, &readfds, NULL, NULL, &timeout);
        if (ret == -1)
        {
            if (errno != EINTR)
            {
                perror("select failed");
                exit(1);
            }
        }
        else if (ret == 0)
            break; /* timeout i.e. no async data */
        else
        {
            /* Received async data; assume it was REQ_ASYNC since nothing
             * else is permitted. The debugger loop will read it and ignore
             * it, so we send the response here.
             */
            gldb_send_code(out_pipe, RESP_STOP);
            debugger_loop(false);
        }
    }
}

static bool debugger_callback(function_call *call, const callback_data *data)
{
    check_async();
    if (break_on[bugle_canonical_call(call)])
    {
        gldb_send_code(out_pipe, RESP_BREAK);
        gldb_send_string(out_pipe, budgie_function_table[call->generic.id].name);
        debugger_loop(false);
    }
    else if (break_on_next)
    {
        break_on_next = false;
        gldb_send_code(out_pipe, RESP_STOP);
        debugger_loop(false);
    }
    return true;
}

static bool debugger_error_callback(function_call *call, const callback_data *data)
{
    GLenum error;

    if (break_on_error
        && (error = bugle_get_call_error(call)))
    {
        gldb_send_code(out_pipe, RESP_BREAK_ERROR);
        gldb_send_string(out_pipe, budgie_function_table[call->generic.id].name);
        gldb_send_string(out_pipe, bugle_gl_enum_to_token(error));
        debugger_loop(false);
    }
    return true;
}

static bool initialise_debugger(filter_set *handle)
{
    const char *env;
    char *last;
    filter *f;

    if (!getenv("BUGLE_DEBUGGER")
        || !getenv("BUGLE_DEBUGGER_FD_IN")
        || !getenv("BUGLE_DEBUGGER_FD_OUT"))
    {
        fputs("The debugger module should only be used with gldb\n", stderr);
        return false;
    }

    env = getenv("BUGLE_DEBUGGER_FD_IN");
    in_pipe = strtol(env, &last, 0);
    if (!*env || *last)
    {
        fprintf(stderr, "Illegal BUGLE_DEBUGGER_FD_IN: '%s' (bug in gldb?)",
                env);
        return false;
    }

    env = getenv("BUGLE_DEBUGGER_FD_OUT");
    out_pipe = strtol(env, &last, 0);
    if (!*env || *last)
    {
        fprintf(stderr, "Illegal BUGLE_DEBUGGER_FD_OUT: '%s' (bug in gldb?)",
                env);
        return false;
    }
    debugger_loop(true);

    f = bugle_register_filter(handle, "debugger");
    bugle_register_filter_catches_all(f, debugger_callback);
    f = bugle_register_filter(handle, "debugger_error");
    bugle_register_filter_catches_all(f, debugger_error_callback);
    bugle_register_filter_depends("invoke", "debugger");
    bugle_register_filter_depends("debugger_error", "invoke");
    bugle_register_filter_depends("debugger_error", "error");
    bugle_register_filter_post_renders("debugger_error");
    bugle_register_filter_set_queries_error("debugger");

    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info debugger_info =
    {
        "debugger",
        initialise_debugger,
        NULL,
        NULL,
        0,
        NULL /* no documentation */
    };

    memset(break_on, 0, sizeof(break_on));
    bugle_register_filter_set(&debugger_info);

    bugle_register_filter_set_depends("debugger", "error");
    bugle_register_filter_set_renders("debugger");
}
