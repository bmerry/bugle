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

void bugle_register_filter_catches_drawing_immediate(filter *f, filter_callback callback)
{
#ifdef GL_ARB_vertex_program
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1sARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1fARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1dARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2sARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2fARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2dARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3sARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3fARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3dARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4sARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4fARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4dARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NubARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1svARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1fvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib1dvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2svARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2fvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib2dvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3svARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3fvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib3dvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4bvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4svARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4ivARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4ubvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4usvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4uivARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4fvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4dvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NbvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NsvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NivARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NubvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NusvARB, callback);
    bugle_register_filter_catches(f, GROUP_glVertexAttrib4NuivARB, callback);
#endif
    bugle_register_filter_catches(f, GROUP_glVertex2d, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2dv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2f, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2fv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2i, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2iv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2s, callback);
    bugle_register_filter_catches(f, GROUP_glVertex2sv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3d, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3dv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3f, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3fv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3i, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3iv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3s, callback);
    bugle_register_filter_catches(f, GROUP_glVertex3sv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4d, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4dv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4f, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4fv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4i, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4iv, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4s, callback);
    bugle_register_filter_catches(f, GROUP_glVertex4sv, callback);

    bugle_register_filter_catches(f, GROUP_glArrayElement, callback);
}

void bugle_register_filter_catches_drawing(filter *f, filter_callback callback)
{
    bugle_register_filter_catches_drawing_immediate(f, callback);

    bugle_register_filter_catches(f, GROUP_glDrawElements, callback);
    bugle_register_filter_catches(f, GROUP_glDrawArrays, callback);
#ifdef GL_EXT_draw_range_elements
    bugle_register_filter_catches(f, GROUP_glDrawRangeElementsEXT, callback);
#endif
#ifdef GL_EXT_multi_draw_arrays
    bugle_register_filter_catches(f, GROUP_glMultiDrawElementsEXT, callback);
    bugle_register_filter_catches(f, GROUP_glMultiDrawArraysEXT, callback);
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
    bugle_register_filter_set_uses_state(name);
    bugle_register_filter_set_depends(name, "trackbeginend");
}

void bugle_register_filter_post_renders(const char *name)
{
    bugle_register_filter_depends(name, "error");
    bugle_register_filter_depends(name, "trackbeginend");
}

void bugle_register_filter_set_uses_state(const char *name)
{
    bugle_register_filter_set_depends(name, "trackcontext");
}

void bugle_register_filter_post_uses_state(const char *name)
{
    bugle_register_filter_depends(name, "trackcontext");
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
