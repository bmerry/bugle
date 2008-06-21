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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/gltypes.h>
#include <bugle/gl/glbeginend.h>
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
#if BUGLE_GLTYPE_GL
    bugle_filter_catches(f, "glVertexAttrib1s", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1f", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1d", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2s", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2f", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2d", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3s", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3f", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3d", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4s", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4f", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4d", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4Nub", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1sv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1fv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib1dv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2sv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2fv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib2dv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3sv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3fv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib3dv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4bv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4sv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4iv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4ubv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4usv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4uiv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4fv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4dv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4Nbv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4Nsv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4Niv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4Nubv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4Nusv", inactive, callback);
    bugle_filter_catches(f, "glVertexAttrib4Nuiv", inactive, callback);
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
#endif /* BUGLE_GLTYPE_GL */
}

void bugle_filter_catches_drawing(filter *f, bool inactive, filter_callback callback)
{
    bugle_filter_catches_drawing_immediate(f, inactive, callback);

    bugle_filter_catches(f, "glDrawElements", inactive, callback);
    bugle_filter_catches(f, "glDrawArrays", inactive, callback);
    bugle_filter_catches(f, "glDrawRangeElements", inactive, callback);
    bugle_filter_catches(f, "glMultiDrawElements", inactive, callback);
    bugle_filter_catches(f, "glMultiDrawArrays", inactive, callback);
}

bool bugle_call_is_immediate(function_call *call)
{
#if BUGLE_GLTYPE_GL
    switch (call->generic.group)
    {
    case GROUP_glVertexAttrib1s:
    case GROUP_glVertexAttrib1f:
    case GROUP_glVertexAttrib1d:
    case GROUP_glVertexAttrib2s:
    case GROUP_glVertexAttrib2f:
    case GROUP_glVertexAttrib2d:
    case GROUP_glVertexAttrib3s:
    case GROUP_glVertexAttrib3f:
    case GROUP_glVertexAttrib3d:
    case GROUP_glVertexAttrib4s:
    case GROUP_glVertexAttrib4f:
    case GROUP_glVertexAttrib4d:
    case GROUP_glVertexAttrib4Nub:
    case GROUP_glVertexAttrib1sv:
    case GROUP_glVertexAttrib1fv:
    case GROUP_glVertexAttrib1dv:
    case GROUP_glVertexAttrib2sv:
    case GROUP_glVertexAttrib2fv:
    case GROUP_glVertexAttrib2dv:
    case GROUP_glVertexAttrib3sv:
    case GROUP_glVertexAttrib3fv:
    case GROUP_glVertexAttrib3dv:
    case GROUP_glVertexAttrib4bv:
    case GROUP_glVertexAttrib4sv:
    case GROUP_glVertexAttrib4iv:
    case GROUP_glVertexAttrib4ubv:
    case GROUP_glVertexAttrib4usv:
    case GROUP_glVertexAttrib4uiv:
    case GROUP_glVertexAttrib4fv:
    case GROUP_glVertexAttrib4dv:
    case GROUP_glVertexAttrib4Nbv:
    case GROUP_glVertexAttrib4Nsv:
    case GROUP_glVertexAttrib4Niv:
    case GROUP_glVertexAttrib4Nubv:
    case GROUP_glVertexAttrib4Nusv:
    case GROUP_glVertexAttrib4Nuiv:
        return (*(GLuint *) call->generic.args[0] == 0);
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
#else /* !BUGLE_GLTYPE_GL */
    return false;
#endif
}

void bugle_filter_set_renders(const char *name)
{
    bugle_filter_set_depends(name, "trackcontext");
    bugle_filter_set_depends(name, "glbeginend");
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
