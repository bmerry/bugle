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
#include "common/bool.h"
#include "filters.h"
#include "glutils.h"
#include "src/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <assert.h>

static state_7context_I **context_state = NULL;
static filter_set *error_handle = NULL;

state_7context_I *get_context_state(void)
{
    assert(context_state);
    return *context_state;
}

bool in_begin_end(void)
{
    assert(context_state);
    return !*context_state || (*context_state)->c_internal.c_in_begin_end.data;
}

bool begin_internal_render(void)
{
    GLenum error;

    if (in_begin_end()) return false;
    /* FIXME: work with the error filterset to save the errors even
     * when the error filterset is not actively checking for errors.
     */
    error = CALL_glGetError();
    if (error != GL_NO_ERROR)
    {
        fputs("An OpenGL error was detected but will be lost to the app.\n"
              "Use the 'error' filterset to allow the app to see it.\n",
              stderr);
        while ((error = CALL_glGetError()) != GL_NO_ERROR);
    }
    return true;
}

void end_internal_render(const char *name, bool warn)
{
    GLenum error;
    while ((error = CALL_glGetError()) != GL_NO_ERROR)
    {
        if (warn)
        {
            fprintf(stderr, "Warning: %s internally generated ", name);
            dump_GLerror(&error, -1, stderr);
            fputs(".\n", stderr);
        }
    }
}

void filter_set_renders(const char *name)
{
    filter_set_uses_state(name);
    register_filter_set_depends(name, "trackbeginend");
}

void filter_post_renders(const char *name)
{
    register_filter_depends(name, "error");
    register_filter_depends(name, "trackbeginend");
}

void filter_set_uses_state(const char *name)
{
    filter_set *handle;

    register_filter_set_depends(name, "trackcontext");
    if (!context_state)
    {
        handle = get_filter_set_handle("trackcontext");
        if (!handle)
        {
            fprintf(stderr, "could not find trackcontext filterset, required by %s filterset\n",
                    name);
            exit(1);
        }
        context_state = (state_7context_I **)
            get_filter_set_symbol(handle, "context_state");
        if (!context_state)
        {
            fprintf(stderr, "could not find symbol context_state in filterset trackcontext\n");
            exit(1);
        }
    }
}

void filter_post_uses_state(const char *name)
{
    register_filter_depends(name, "trackcontext");
}

void filter_set_queries_error(const char *name, bool require)
{
    if (require) register_filter_set_depends(name, "error");
    if (!error_handle)
        error_handle = get_filter_set_handle("error");
}

GLenum get_call_error(function_call *call)
{
    if (error_handle
        && filter_set_is_enabled(error_handle))
        return *(GLenum *) get_filter_set_call_state(call, error_handle);
    else
        return GL_NO_ERROR;
}
