#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/filters.h"
#include "src/utils.h"
#include "src/types.h"
#include "src/canon.h"
#include "budgielib/state.h"
#include "common/bool.h"
#include <stdio.h>

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

    /* FIXME: begin/end handling */
    /* FIXME: work with the log module */
    if (function_table[call->generic.id].name[2] == 'X') return true; /* GLX */
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
    register_filter(handle, "error_pre", error_pre_callback);
    register_filter(handle, "error_post", error_post_callback);
    register_filter_depends("invoke", "error_pre");
    register_filter_depends("error_post", "invoke");
    register_filter_set_depends("error", "trackcontext");
    get_context_state = (state_7context_I *(*)(void))
        get_filter_set_symbol(get_filter_set_handle("trackcontext"),
                              "get_context_state");
    return true;
}

void initialise_filter_library(void)
{
    register_filter_set("error", initialise_error, NULL);
}
