/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <assert.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/gltypes.h>
#include <bugle/gl/trackbeginend.h>
#include <bugle/apireflect.h>
#include <bugle/filters.h>
#include <bugle/log.h>
#include <budgie/call.h>
#include "budgielib/defines.h"

static filter_set *error_handle = NULL;
static GLenum (*bugle_call_get_error_ptr)(object *) = NULL;

bool bugle_begin_internal_render(void)
{
    GLenum error;

    if (bugle_gl_in_begin_end()) return false;
    /* FIXME: work with the error filterset to save the errors even
     * when the error filterset is not actively checking for errors.
     */
    error = CALL(glGetError)();
    if (error != GL_NO_ERROR)
    {
        bugle_log("glutils", "internalrender", BUGLE_LOG_WARNING,
                  "An OpenGL error was detected but will be lost to the application.");
        bugle_log("glutils", "internalrender", BUGLE_LOG_WARNING,
                  "Use the 'error' filterset to allow the application to see errors.");
        while ((error = CALL(glGetError)()) != GL_NO_ERROR);
    }
    return true;
}

void bugle_end_internal_render(const char *name, bool warn)
{
    GLenum error;
    while ((error = CALL(glGetError)()) != GL_NO_ERROR)
    {
        if (warn)
        {
            const char *error_name;
            error_name = bugle_api_enum_name(error);
            if (error_name)
                bugle_log_printf("glutils", "internalrender", BUGLE_LOG_WARNING,
                                 "%s internally generated %s",
                                 name, error_name);
            else
                bugle_log_printf("glutils", "internalrender", BUGLE_LOG_WARNING,
                                 "%s internally generated error %#08x",
                                 name, (unsigned int) error);
        }
    }
}

void bugle_filter_catches_drawing_immediate(filter *f, bool inactive, filter_callback callback)
{
#ifdef GL_ARB_vertex_program
    bugle_filter_catches(f, "glVertexAttrib1sARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1fARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1dARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2sARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2fARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2dARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3sARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3fARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3dARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4sARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4fARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4dARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4NubARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1svARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1fvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1dvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2svARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2fvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2dvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3svARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3fvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3dvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4bvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4svARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4ivARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4ubvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4usvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4uivARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4fvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4dvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4NbvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4NsvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4NivARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4NubvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4NusvARB", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4NuivARB", inactive, callback);
#endif
    bugle_filter_catches(f, "glVertex2d", inactive, callback);
    bugle_filter_catches(f, "glVertex2dv", inactive, callback);
    bugle_filter_catches(f, "glVertex2f", inactive, callback);
    bugle_filter_catches(f, "glVertex2fv", inactive, callback);
    bugle_filter_catches(f, "glVertex2i", inactive, callback);
    bugle_filter_catches(f, "glVertex2iv", inactive, callback);
    bugle_filter_catches(f, "glVertex2s", inactive, callback);
    bugle_filter_catches(f, "glVertex2sv", inactive, callback);
    bugle_filter_catches(f, "glVertex3d", inactive, callback);
    bugle_filter_catches(f, "glVertex3dv", inactive, callback);
    bugle_filter_catches(f, "glVertex3f", inactive, callback);
    bugle_filter_catches(f, "glVertex3fv", inactive, callback);
    bugle_filter_catches(f, "glVertex3i", inactive, callback);
    bugle_filter_catches(f, "glVertex3iv", inactive, callback);
    bugle_filter_catches(f, "glVertex3s", inactive, callback);
    bugle_filter_catches(f, "glVertex3sv", inactive, callback);
    bugle_filter_catches(f, "glVertex4d", inactive, callback);
    bugle_filter_catches(f, "glVertex4dv", inactive, callback);
    bugle_filter_catches(f, "glVertex4f", inactive, callback);
    bugle_filter_catches(f, "glVertex4fv", inactive, callback);
    bugle_filter_catches(f, "glVertex4i", inactive, callback);
    bugle_filter_catches(f, "glVertex4iv", inactive, callback);
    bugle_filter_catches(f, "glVertex4s", inactive, callback);
    bugle_filter_catches(f, "glVertex4sv", inactive, callback);

    bugle_filter_catches(f, "glArrayElement", inactive, callback);
}

void bugle_filter_catches_drawing(filter *f, bool inactive, filter_callback callback)
{
    bugle_filter_catches_drawing_immediate(f, inactive, callback);

    bugle_filter_catches(f, "glDrawElements", inactive, callback);
    bugle_filter_catches(f, "glDrawArrays", inactive, callback);
#ifdef GL_EXT_draw_range_elements
    bugle_filter_catches(f, "glDrawRangeElementsEXT", inactive, callback);
#endif
#ifdef GL_EXT_multi_draw_arrays
    bugle_filter_catches(f, "glMultiDrawElementsEXT", inactive, callback);
    bugle_filter_catches(f, "glMultiDrawArraysEXT", inactive, callback);
#endif
}

bool bugle_call_is_immediate(function_call *call)
{
    switch (call->generic.group)
    {
#ifdef GL_ARB_vertex_program
    case GROUP_glVertexAttrib1sARB:
    case GROUP_glVertexAttrib1fARB:
    case GROUP_glVertexAttrib1dARB:
    case GROUP_glVertexAttrib2sARB:
    case GROUP_glVertexAttrib2fARB:
    case GROUP_glVertexAttrib2dARB:
    case GROUP_glVertexAttrib3sARB:
    case GROUP_glVertexAttrib3fARB:
    case GROUP_glVertexAttrib3dARB:
    case GROUP_glVertexAttrib4sARB:
    case GROUP_glVertexAttrib4fARB:
    case GROUP_glVertexAttrib4dARB:
    case GROUP_glVertexAttrib4NubARB:
    case GROUP_glVertexAttrib1svARB:
    case GROUP_glVertexAttrib1fvARB:
    case GROUP_glVertexAttrib1dvARB:
    case GROUP_glVertexAttrib2svARB:
    case GROUP_glVertexAttrib2fvARB:
    case GROUP_glVertexAttrib2dvARB:
    case GROUP_glVertexAttrib3svARB:
    case GROUP_glVertexAttrib3fvARB:
    case GROUP_glVertexAttrib3dvARB:
    case GROUP_glVertexAttrib4bvARB:
    case GROUP_glVertexAttrib4svARB:
    case GROUP_glVertexAttrib4ivARB:
    case GROUP_glVertexAttrib4ubvARB:
    case GROUP_glVertexAttrib4usvARB:
    case GROUP_glVertexAttrib4uivARB:
    case GROUP_glVertexAttrib4fvARB:
    case GROUP_glVertexAttrib4dvARB:
    case GROUP_glVertexAttrib4NbvARB:
    case GROUP_glVertexAttrib4NsvARB:
    case GROUP_glVertexAttrib4NivARB:
    case GROUP_glVertexAttrib4NubvARB:
    case GROUP_glVertexAttrib4NusvARB:
    case GROUP_glVertexAttrib4NuivARB:
        return (*(GLuint *) call->generic.args[0] == 0);
#endif
    case GROUP_glVertex2d:
    case GROUP_glVertex2dv:
    case GROUP_glVertex2f:
    case GROUP_glVertex2fv:
    case GROUP_glVertex2i:
    case GROUP_glVertex2iv:
    case GROUP_glVertex2s:
    case GROUP_glVertex2sv:
    case GROUP_glVertex3d:
    case GROUP_glVertex3dv:
    case GROUP_glVertex3f:
    case GROUP_glVertex3fv:
    case GROUP_glVertex3i:
    case GROUP_glVertex3iv:
    case GROUP_glVertex3s:
    case GROUP_glVertex3sv:
    case GROUP_glVertex4d:
    case GROUP_glVertex4dv:
    case GROUP_glVertex4f:
    case GROUP_glVertex4fv:
    case GROUP_glVertex4i:
    case GROUP_glVertex4iv:
    case GROUP_glVertex4s:
    case GROUP_glVertex4sv:
    case GROUP_glArrayElement:
        return true;
    default:
        return false;
    }
}

void bugle_filter_set_renders(const char *name)
{
    bugle_filter_set_depends(name, "trackcontext");
    bugle_filter_set_depends(name, "trackbeginend");
    bugle_filter_set_depends(name, "log");
}

void bugle_filter_post_renders(const char *name)
{
    bugle_filter_order("error", name);
    bugle_gl_filter_post_queries_begin_end(name);
}

void bugle_filter_set_queries_error(const char *name)
{
    if (!error_handle)
    {
        error_handle = bugle_filter_set_get_handle("error");
        bugle_call_get_error_ptr = (GLenum (*)(object *)) bugle_filter_set_get_symbol(error_handle, "bugle_call_get_error_internal");
    }
}

GLenum bugle_call_get_error(object *call_object)
{
    if (error_handle
        && bugle_filter_set_is_active(error_handle)
        && bugle_call_get_error_ptr)
        return bugle_call_get_error_ptr(call_object);
    else
        return GL_NO_ERROR;
}
