#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/filters.h"
#include "src/utils.h"
#include "src/types.h"
#include "src/canon.h"
#include "budgielib/state.h"
#include "common/bool.h"
#include <assert.h>

static state_7context_I *context_state = NULL;

state_7context_I *get_context_state(void)
{
    return context_state;
}

bool trackcontext_callback(function_call *call, void *data)
{
    GLXContext ctx;
    state_generic *parent;

    switch (canonical_call(call))
    {
    case FUNC_glXMakeCurrent:
        ctx = *call->typed.glXMakeCurrent.arg2;
        break;
    case FUNC_glXMakeContextCurrent:
        ctx = *call->typed.glXMakeContextCurrent.arg3;
        break;
    default:
        return true;
    }
    parent = &get_root_state()->c_context.generic;
    if (!(context_state = (state_7context_I *) get_state_index(parent, ctx)))
        context_state = (state_7context_I *) add_state_index(parent, ctx);
    return true;
}

/* Track whether we are in glBegin/glEnd. This is trickier than it looks,
 * because glBegin can fail if given an illegal primitive, and we can't
 * check for the error because glGetError is illegal inside glBegin/glEnd.
 */
static bool trackbeginend_callback(function_call *call, void *data)
{
    switch (canonical_call(call))
    {
    case FUNC_glBegin:
        switch (*call->typed.glBegin.arg0)
        {
        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_QUADS:
        case GL_QUAD_STRIP:
        case GL_POLYGON:
            assert(context_state);
            context_state->c_internal.c_in_begin_end.data = GL_TRUE;
        }
    case FUNC_glEnd:
        assert(context_state);
        context_state->c_internal.c_in_begin_end.data = GL_FALSE;
    }
    return true;
}

static bool initialise_trackcontext(filter_set *handle)
{
    register_filter(handle, "trackcontext", trackcontext_callback);
    register_filter_depends("trackcontext", "invoke");
    return true;
}

static bool initialise_trackbeginend(filter_set *handle)
{
    register_filter(handle, "trackbeginend", trackbeginend_callback);
    register_filter_depends("trackbeginend", "invoke");
    register_filter_set_depends("trackbeginend", "trackcontext");
    return true;
}

void initialise_filter_library(void)
{
    register_filter_set("trackcontext", initialise_trackcontext, NULL, NULL);
    register_filter_set("trackbeginend", initialise_trackbeginend, NULL, NULL);
}
