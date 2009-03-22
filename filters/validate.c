/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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
#define _POSIX_SOURCE
#define GL_GLEXT_PROTOTYPES
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glbeginend.h>
#include <bugle/filters.h>
#include <bugle/log.h>
#include <bugle/apireflect.h>
#include "common/threads.h"
#include <budgie/addresses.h>
#include <budgie/types.h>
#include <budgie/reflect.h>

static bool trap = false;
static filter_set *error_handle = NULL;
static object_view error_context_view, error_call_view;

GLenum bugle_gl_call_get_error_internal(object *call_object)
{
    GLenum *call_error;
    call_error = bugle_object_get_data(call_object, error_call_view);
    return call_error ? *call_error : GL_NO_ERROR;
}

static bool error_callback(function_call *call, const callback_data *data)
{
    GLenum error;
    GLenum *stored_error;
    GLenum *call_error;

    stored_error = bugle_object_get_current_data(bugle_context_class, error_context_view);
    call_error = bugle_object_get_current_data(bugle_call_class, error_call_view);
    *call_error = GL_NO_ERROR;

    if (bugle_api_extension_block(bugle_api_function_extension(call->generic.id)) != BUGLE_API_EXTENSION_BLOCK_GL)
        return true;  /* only applies to real GL calls */
    if (call->generic.group == BUDGIE_GROUP_ID(glGetError))
    {
        /* We hope that it returns GL_NO_ERROR, since otherwise something
         * slipped through our own net. If not, we return it to the app
         * rather than whatever we have saved. Also, we must make sure to
         * return nothing else inside begin/end.
         */
        if (*call->glGetError.retn != GL_NO_ERROR)
        {
            const char *name;
            name = bugle_api_enum_name(*call->glGetError.retn, BUGLE_API_EXTENSION_BLOCK_GL);
            if (name)
                bugle_log_printf("error", "callback", BUGLE_LOG_WARNING,
                                 "glGetError() returned %s when GL_NO_ERROR was expected",
                                 name);
            else
                bugle_log_printf("error", "callback", BUGLE_LOG_WARNING,
                                 "glGetError() returned %#08x when GL_NO_ERROR was expected",
                                 (unsigned int) *call->glGetError.retn);
        }
        else if (bugle_gl_in_begin_end())
        {
            *call_error = GL_INVALID_OPERATION;
        }
        else if (*stored_error)
        {
            *call->glGetError.retn = *stored_error;
            *stored_error = GL_NO_ERROR;
        }
    }
    else if (!bugle_gl_in_begin_end())
    {
        /* Note: we deliberately don't call begin_internal_render here,
         * since it will beat us to calling glGetError().
         */
        while ((error = CALL(glGetError)()) != GL_NO_ERROR)
        {
            if (stored_error && !*stored_error)
                *stored_error = error;
            *call_error = error;
            if (trap && bugle_filter_set_is_active(data->filter_set_handle))
            {
                fflush(stderr);
                /* SIGTRAP is technically a BSD extension, and various
                 * versions of FreeBSD do weird things (e.g. 4.8 will
                 * never define it if _POSIX_SOURCE is defined). Rather
                 * than try all possibilities we just SIGABRT instead.
                 */
#ifdef SIGTRAP
                bugle_thread_raise(SIGTRAP);
#else
                abort();
#endif
            }
        }
    }
    return true;
}

static bool error_initialise(filter_set *handle)
{
    filter *f;

    error_handle = handle;
    f = bugle_filter_new(handle, "error");
    bugle_filter_catches_all(f, true, error_callback);
    bugle_filter_order("invoke", "error");
    bugle_gl_filter_post_queries_begin_end("error");
    /* We don't call filter_post_renders, because that would make the
     * error filter-set depend on itself.
     */
    error_context_view = bugle_object_view_new(bugle_context_class,
                                               NULL,
                                               NULL,
                                               sizeof(GLenum));
    error_call_view = bugle_object_view_new(bugle_call_class,
                                            NULL,
                                            NULL,
                                            sizeof(GLenum));
    return true;
}

static bool showerror_callback(function_call *call, const callback_data *data)
{
    GLenum error;
    if ((error = bugle_gl_call_get_error_internal(data->call_object)) != GL_NO_ERROR)
    {
        const char *name;
        name = bugle_api_enum_name(error, BUGLE_API_EXTENSION_BLOCK_GL);
        if (name)
            bugle_log_printf("showerror", "gl", BUGLE_LOG_NOTICE,
                             "%s in %s", name,
                             budgie_function_name(call->generic.id));
        else
            bugle_log_printf("showerror", "gl", BUGLE_LOG_NOTICE,
                             "%#08x in %s", (unsigned int) error,
                             budgie_function_name(call->generic.id));
    }
    return true;
}

static bool showerror_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "showerror");
    bugle_filter_catches_all(f, false, showerror_callback);
    bugle_filter_order("error", "showerror");
    bugle_filter_order("invoke", "showerror");
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info error_info =
    {
        "error",
        error_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "checks for OpenGL errors after each call (see also `showerror')"
    };
    static const filter_set_info showerror_info =
    {
        "showerror",
        showerror_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "logs OpenGL errors"
    };

    bugle_filter_set_new(&error_info);
    bugle_filter_set_new(&showerror_info);

    bugle_gl_filter_set_renders("error");
    bugle_filter_set_depends("showerror", "error");
    bugle_gl_filter_set_queries_error("showerror");
}
