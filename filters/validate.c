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
#include "src/filters.h"
#include "src/utils.h"
#include "src/types.h"
#include "src/canon.h"
#include "budgielib/state.h"
#include "common/bool.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>

static state_7context_I *(*get_context_state)(void);

static bool error_pre_callback(function_call *call, void *data)
{
    state_7context_I *ctx;

    ctx = (*get_context_state)();
    if (canonical_call(call) == FUNC_glGetError
        && ctx && !ctx->c_internal.c_in_begin_end.data
        && ctx->c_internal.c_error.data)
    {
        *call->typed.glGetError.retn = ctx->c_internal.c_error.data;
        ctx->c_internal.c_error.data = GL_NO_ERROR;
        return false;
    }
    return true;
}

static bool error_post_callback(function_call *call, void *data)
{
    state_7context_I *ctx;
    GLenum error;

    /* FIXME: work with the log module */
    if (function_table[call->generic.id].name[2] == 'X') return true; /* GLX */
    assert(get_context_state);
    ctx = (*get_context_state)();
    if (ctx && !ctx->c_internal.c_in_begin_end.data)
    {
        error = (*glGetError_real)();
        if (error)
        {
            dump_any_type(TYPE_7GLerror, &error, 1, stderr);
            fprintf(stderr, " in %s\n", function_table[call->generic.id].name);
            if (ctx && !ctx->c_internal.c_error.data)
                ctx->c_internal.c_error.data = error;
        }
    }
    return true;
}

static bool initialise_error(filter_set *handle)
{
    filter_set *trackcontext;

    register_filter(handle, "error_pre", error_pre_callback);
    register_filter(handle, "error_post", error_post_callback);
    register_filter_depends("invoke", "error_pre");
    register_filter_depends("error_post", "invoke");
    register_filter_depends("error_post", "trackbeginend");
    register_filter_set_depends("error", "trackbeginend");
    register_filter_set_depends("error", "trackcontext");

    trackcontext = get_filter_set_handle("trackcontext");
    if (!trackcontext)
    {
        fputs("trackcontext filter-set not found (required by error)\n", stderr);
        return false;
    }
    get_context_state = (state_7context_I *(*)(void))
        get_filter_set_symbol(trackcontext, "get_context_state");
    return true;
}

/* Stack unwind hack, to get a usable stack trace after a segfault inside
 * the OpenGL driver, if it was compiled with -fomit-frame-pointer (such
 * as the NVIDIA drivers). This implementation violates the requirement
 * that the function calling setjmp shouldn't return (see setjmp(3)), but
 * It Works For Me (tm). Unfortunately there doesn't seem to be a way
 * to satisfy the requirements of setjmp without breaking the nicely
 * modular filter system.
 */
static struct sigaction old_sigsegv_act;
static sigjmp_buf unwind_buf;

static void unwindstack_sigsegv_handler(int sig)
{
    siglongjmp(unwind_buf, 1);
}

static bool unwindstack_pre_callback(function_call *call, void *data)
{
    struct sigaction act;

    if (sigsetjmp(unwind_buf, 1))
    {
        fputs("A segfault occurred, which was caught by the unwindstack\n"
              "filter-set. It will now be rethrown. If you are running in\n"
              "a debugger, you should get a useful stack trace. Do not\n"
              "try to continue again, as gdb will get confused.\n", stderr);
        fflush(stderr);
        /* avoid hitting the same handler */
        while (sigaction(SIGSEGV, &old_sigsegv_act, NULL) != 0)
            if (errno != EINTR)
            {
                perror("failed to set SIGSEGV handler");
                exit(1);
            }
        raise(SIGSEGV);
        exit(1); /* make sure we don't recover */
    }
    act.sa_handler = unwindstack_sigsegv_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    while (sigaction(SIGSEGV, &act, &old_sigsegv_act) != 0)
        if (errno != EINTR)
        {
            perror("failed to set SIGSEGV handler");
            exit(1);
        }
    return true;
}

static bool unwindstack_post_callback(function_call *call, void *data)
{
    while (sigaction(SIGSEGV, &old_sigsegv_act, NULL) != 0)
        if (errno != EINTR)
        {
            perror("failed to restore SIGSEGV handler");
            exit(1);
        }
    return true;
}

static bool initialise_unwindstack(filter_set *handle)
{
    register_filter(handle, "unwindstack_pre", unwindstack_pre_callback);
    register_filter(handle, "unwindstack_post", unwindstack_post_callback);
    register_filter_depends("unwindstack_post", "invoke");
    register_filter_depends("invoke", "unwindstack_pre");
    return true;
}

void initialise_filter_library(void)
{
    register_filter_set("error", initialise_error, NULL, NULL);
    register_filter_set("unwindstack", initialise_unwindstack, NULL, NULL);
}
