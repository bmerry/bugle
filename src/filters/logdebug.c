/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2012  Bruce Merry
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
#include <bugle/filters.h>
#include <bugle/log.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glextensions.h>
#include <bugle/gl/glutils.h>
#include <bugle/objects.h>
#include <budgie/call.h>
#include <budgie/callapi.h>

static object_view logdebug_view;
static bugle_bool logdebug_sync = BUGLE_FALSE;

typedef struct
{
    bugle_bool supported;
    bugle_bool active;
    GLDEBUGPROCARB orig_callback;
    GLvoid *orig_user_param;
} logdebug_context;

static void BUDGIEAPI logdebug_message(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar *message, GLvoid *user_param)
{
    logdebug_context *ctx = (logdebug_context *) user_param;

    int level;
    const char *source_label;
    const char *type_label;

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH_ARB:
        level = BUGLE_LOG_WARNING;
        break;
    case GL_DEBUG_SEVERITY_MEDIUM_ARB:
        level = BUGLE_LOG_NOTICE;
        break;
    case GL_DEBUG_SEVERITY_LOW_ARB:
        level = BUGLE_LOG_INFO;
        break;
    default:
        level = BUGLE_LOG_INFO; /* should never be reached */
    }

    switch (source)
    {
    case GL_DEBUG_SOURCE_API_ARB:             source_label = "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:   source_label = "window system"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: source_label = "shader compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:     source_label = "third party"; break;
    case GL_DEBUG_SOURCE_APPLICATION_ARB:     source_label = "application"; break;
    case GL_DEBUG_SOURCE_OTHER_ARB:           source_label = "other"; break;
    default:                                  source_label = "???"; break;
    }

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR_ARB:               type_label = "error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: type_label = "deprecated behavior"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:  type_label = "undefined behavior"; break;
    case GL_DEBUG_TYPE_PORTABILITY_ARB:         type_label = "portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE_ARB:         type_label = "performance"; break;
    case GL_DEBUG_TYPE_OTHER_ARB:               type_label = "other"; break;
    default:                                    type_label = "???"; break;
    }

    bugle_log_printf("logdebug", "message", level,
                     "%.*s [source: %s type: %s id: %u]",
                     length, message, source_label, type_label, (unsigned int) id);

    if (ctx->orig_callback != NULL)
    {
        (*ctx->orig_callback)(source, type, id, severity, length, message, ctx->orig_user_param);
    }
}

static void logdebug_context_init(const void *key, void *data)
{
    logdebug_context *ctx = (logdebug_context *) data;
    ctx->supported = BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_debug_output);
}

static void logdebug_handle_activation(bugle_bool active)
{
    logdebug_context *ctx;

    ctx = (logdebug_context *) bugle_object_get_current_data(bugle_get_context_class(), logdebug_view);
    if (ctx)
    {
        if (!ctx->supported)
            return;

        if (active && !ctx->active)
        {
            if (bugle_gl_begin_internal_render())
            {
                CALL(glGetPointerv)(GL_DEBUG_CALLBACK_FUNCTION_ARB, (GLvoid **) &ctx->orig_callback);
                CALL(glGetPointerv)(GL_DEBUG_CALLBACK_USER_PARAM_ARB, &ctx->orig_user_param);
                /* Enable all messages */
                CALL(glDebugMessageControlARB)(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
                CALL(glDebugMessageCallbackARB)(logdebug_message, ctx);
                if (logdebug_sync)
                {
                    CALL(glEnable)(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
                }
                bugle_gl_end_internal_render("logdebug_handle_activation", BUGLE_TRUE);
            }
            ctx->active = BUGLE_TRUE;
        }
        else if (!active && ctx->active)
        {
            /* TODO: should record the prior state of all debug messages */
            if (bugle_gl_begin_internal_render())
            {
                if (logdebug_sync)
                {
                    CALL(glDisable)(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
                }
                CALL(glDebugMessageControlARB)(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_FALSE);
                CALL(glDebugMessageCallbackARB)(ctx->orig_callback, ctx->orig_user_param);
                bugle_gl_end_internal_render("logdebug_handle_activation", BUGLE_TRUE);
            }
            ctx->active = BUGLE_FALSE;
        }
    }
}

static void logdebug_activation(filter_set *handle)
{
    (void) handle;
    logdebug_handle_activation(BUGLE_TRUE);
}

static void logdebug_deactivation(filter_set *handle)
{
    (void) handle;
    logdebug_handle_activation(BUGLE_FALSE);
}

/* This is called even when inactive, to restore the proper state to contexts
 * that were not current when activation or deactivation occurred.
 */
static bugle_bool logdebug_make_current(function_call *call, const callback_data *data)
{
    logdebug_handle_activation(bugle_filter_set_is_active(data->filter_set_handle));
    return BUGLE_TRUE;
}

static bugle_bool logdebug_glDebugMessageControlARB(function_call *call, const callback_data *data)
{
    /* The function may have turned off some of our messages. Turn them back on.
     */
    if (bugle_gl_begin_internal_render())
    {
        CALL(glDebugMessageControlARB)(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
        bugle_gl_end_internal_render("logdebug_glDebugMessageControlARB", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool logdebug_glDebugMessageCallbackARB(function_call *call, const callback_data *data)
{
    /* The function may have replaced our callbacks. Restore ours and save the
     * user's so they can be restored during deactivation.
     */
    if (bugle_gl_begin_internal_render())
    {
        GLvoid *callback, *user_param;
        CALL(glGetPointerv)(GL_DEBUG_CALLBACK_FUNCTION_ARB, &callback);
        CALL(glGetPointerv)(GL_DEBUG_CALLBACK_USER_PARAM_ARB, &user_param);
        /* The call may have failed, in which case we'll get our own function
         * back. No action needed in that case.
         */
        if (callback != logdebug_message)
        {
            logdebug_context *ctx;
            ctx = (logdebug_context *) bugle_object_get_current_data(bugle_get_context_class(), logdebug_view);
            CALL(glDebugMessageCallbackARB)(logdebug_message, ctx);

            ctx->orig_callback = (GLDEBUGPROCARB) callback;
            ctx->orig_user_param = user_param;
        }
        bugle_gl_end_internal_render("logdebug_glDebugMessageCallbackARB", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool logdebug_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "logdebug");
    bugle_glwin_filter_catches_make_current(f, BUGLE_TRUE, logdebug_make_current);
    bugle_filter_catches(f, "glDebugMessageControlARB", BUGLE_FALSE, logdebug_glDebugMessageControlARB);
    bugle_filter_catches(f, "glDebugMessageCallbackARB", BUGLE_FALSE, logdebug_glDebugMessageCallbackARB);

    bugle_filter_order("invoke", "logdebug");
    bugle_gl_filter_post_renders("logdebug");

    logdebug_view = bugle_object_view_new(bugle_get_context_class(),
                                          logdebug_context_init,
                                          NULL,
                                          sizeof(logdebug_context));

    return BUGLE_TRUE;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info logdebug_variables[] =
    {
        { "sync", "enable GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB [no]", FILTER_SET_VARIABLE_BOOL, &logdebug_sync, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };
    
    static const filter_set_info logdebug_info =
    {
        "logdebug",
        logdebug_initialise,
        NULL,
        logdebug_activation,
        logdebug_deactivation,
        logdebug_variables,
        "Send ARB_debug_output messages to the BuGLe log"
    };

    bugle_filter_set_new(&logdebug_info);
    bugle_gl_filter_set_renders("logdebug");
    bugle_filter_set_depends("logdebug", "trackcontext");
    bugle_filter_set_depends("logdebug", "glextensions");
    bugle_gl_filter_set_renders("logdebug");
}
