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
#include "src/glutils.h"
#include "budgielib/state.h"
#include "common/bool.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>

static bool trap = false;

static bool error_callback(function_call *call, void *data)
{
    state_7context_I *ctx;
    GLenum error;

    *(GLenum *) data = GL_NO_ERROR;
    if (function_table[call->generic.id].name[2] == 'X') return true; /* GLX */
    ctx = get_context_state();
    if (canonical_call(call) == FUNC_glGetError)
    {
        /* We hope that it returns GL_NO_ERROR, since otherwise something
         * slipped through our own net. If not, we return it to the app
         * rather than whatever we have saved. Also, we must make sure to
         * return nothing else inside begin/end.
         */
        if (*call->typed.glGetError.retn != GL_NO_ERROR)
        {
            fputs("Warning: glGetError() returned ", stderr);
            dump_GLerror(call->typed.glGetError.retn, -1, stderr);
            fputs("\n", stderr);
        }
        else if (!in_begin_end() && ctx->c_internal.c_error.data)
        {
            *call->typed.glGetError.retn = ctx->c_internal.c_error.data;
            ctx->c_internal.c_error.data = GL_NO_ERROR;
        }
    }
    else if (!in_begin_end())
    {
        /* Note: we deliberately don't call begin_internal_render here,
         * since it will beat us to calling glGetError().
         */
        while ((error = CALL_glGetError()) != GL_NO_ERROR)
        {
            dump_any_type(TYPE_7GLerror, &error, 1, stderr);
            fprintf(stderr, " in %s\n", function_table[call->generic.id].name);
            if (ctx && !ctx->c_internal.c_error.data)
                ctx->c_internal.c_error.data = error;
            *(GLenum *) data = error;
            if (trap)
            {
                fflush(stderr);
                raise(SIGTRAP);
            }
        }
    }
    return true;
}

static bool initialise_error(filter_set *handle)
{
    register_filter(handle, "error", error_callback);
    register_filter_depends("error", "invoke");
    register_filter_set_call_state(handle, sizeof(GLenum));
    /* We don't call filter_post_renders, because that would make the
     * error filterset depend on itself.
     */
    filter_set_renders("error");
    return true;
}

static bool set_variable_error(filter_set *handle,
                               const char *name,
                               const char *value)
{
    if (strcmp(name, "trap") == 0)
    {
        if (strcmp(value, "yes") == 0)
            trap = true;
        else if (strcmp(value, "no") == 0)
            trap = false;
        else
            fprintf(stderr, "please specify trap as either yes or no");
        return true;
    }
    else return false;
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
    register_filter_set("error", initialise_error, NULL, set_variable_error);
    register_filter_set("unwindstack", initialise_unwindstack, NULL, NULL);
}
