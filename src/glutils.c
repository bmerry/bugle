/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
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
#include "src/filters.h"
#include "src/glutils.h"
#include "src/tracker.h"
#include "src/utils.h"
#include "src/glfuncs.h"
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <assert.h>

static filter_set *error_handle = NULL;

bool bugle_begin_internal_render(void)
{
    GLenum error;

    if (bugle_in_begin_end()) return false;
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

void bugle_end_internal_render(const char *name, bool warn)
{
    GLenum error;
    while ((error = CALL_glGetError()) != GL_NO_ERROR)
    {
        if (warn)
        {
            fprintf(stderr, "Warning: %s internally generated ", name);
            bugle_dump_GLerror(error, stderr);
            fputs(".\n", stderr);
        }
    }
}

void bugle_register_filter_catches_drawing_immediate(filter *f, bool inactive, filter_callback callback)
{
#ifdef GL_ARB_vertex_program
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1sARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1fARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1dARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2sARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2fARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2dARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3sARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3fARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3dARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4sARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4fARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4dARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NubARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1svARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1fvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1dvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2svARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2fvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2dvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3svARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3fvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3dvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4bvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4svARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4ivARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4ubvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4usvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4uivARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4fvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4dvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NbvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NsvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NivARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NubvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NusvARB, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NuivARB, inactive, callback);
#endif
    bugle_register_filter_catches(f, GROUP_glVertex2d, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2dv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2f, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2fv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2i, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2iv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2s, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2sv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3d, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3dv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3f, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3fv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3i, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3iv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3s, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3sv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4d, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4dv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4f, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4fv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4i, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4iv, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4s, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4sv, inactive, callback);

    bugle_register_filter_catches(f, GROUP_glArrayElement, inactive, callback);
}

void bugle_register_filter_catches_drawing(filter *f, bool inactive, filter_callback callback)
{
    bugle_register_filter_catches_drawing_immediate(f, inactive, callback);

    bugle_register_filter_catches(f, GROUP_glDrawElements, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glDrawArrays, inactive, callback);
#ifdef GL_EXT_draw_range_elements
    bugle_register_filter_catches(f, GROUP_glDrawRangeElementsEXT, inactive, callback);
#endif
#ifdef GL_EXT_multi_draw_arrays
    bugle_register_filter_catches(f, GROUP_glMultiDrawElementsEXT, inactive, callback);
    bugle_register_filter_catches(f, GROUP_glMultiDrawArraysEXT, inactive, callback);
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

void bugle_register_filter_set_renders(const char *name)
{
    bugle_register_filter_set_depends(name, "trackcontext");
    bugle_register_filter_set_depends(name, "trackbeginend");
}

void bugle_register_filter_post_renders(const char *name)
{
    bugle_register_filter_depends(name, "error");
    bugle_register_filter_depends(name, "trackbeginend");
}

void bugle_register_filter_set_queries_error(const char *name)
{
    if (!error_handle)
        error_handle = bugle_get_filter_set_handle("error");
}

GLenum bugle_get_call_error(function_call *call)
{
    if (error_handle
        && bugle_filter_set_is_enabled(error_handle))
        return *(GLenum *) bugle_get_filter_set_call_state(call, error_handle);
    else
        return GL_NO_ERROR;
}
