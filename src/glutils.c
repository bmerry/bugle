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
#include "src/canon.h"
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

GLXContext bugle_get_aux_context()
{
    state_7context_I *context_state;
    GLXContext old_ctx, ctx;
    int render_type = 0, screen;
    int n;
    int attribs[3] = {GLX_FBCONFIG_ID, 0, None};
    GLXFBConfig *cfgs;
    Display *dpy;

    context_state = bugle_tracker_get_context_state();
    if (context_state->c_internal.c_auxcontext.data == NULL)
    {
        dpy = CALL_glXGetCurrentDisplay();
        old_ctx = CALL_glXGetCurrentContext();
        CALL_glXQueryContext(dpy, old_ctx, GLX_RENDER_TYPE, &render_type);
        CALL_glXQueryContext(dpy, old_ctx, GLX_SCREEN, &screen);
        CALL_glXQueryContext(dpy, old_ctx, GLX_FBCONFIG_ID, &attribs[1]);
        /* It seems that render_type comes back as a boolean, although
         * the spec seems to indicate that it should be otherwise.
         */
        if (render_type <= 1)
            render_type = render_type ? GLX_RGBA_TYPE : GLX_COLOR_INDEX_TYPE;
        cfgs = CALL_glXChooseFBConfig(dpy, screen, attribs, &n);
        if (cfgs == NULL)
        {
            fprintf(stderr, "Warning: could not create an auxiliary context\n");
            return NULL;
        }
        ctx = CALL_glXCreateNewContext(dpy, cfgs[0], render_type,
                                       old_ctx, CALL_glXIsDirect(dpy, old_ctx));
        XFree(cfgs);
        if (ctx == NULL)
            fprintf(stderr, "Warning: could not create an auxiliary context\n");
        context_state->c_internal.c_auxcontext.data = ctx;
    }
    return context_state->c_internal.c_auxcontext.data;
}

void bugle_register_filter_catches_drawing_immediate(filter *f, filter_callback callback)
{
#ifdef GL_ARB_vertex_program
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib1sARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib1fARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib1dARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib2sARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib2fARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib2dARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib3sARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib3fARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib3dARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4sARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4fARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4dARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NubARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib1svARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib1fvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib1dvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib2svARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib2fvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib2dvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib3svARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib3fvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib3dvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4bvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4svARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4ivARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4ubvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4usvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4uivARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4fvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4dvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NbvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NsvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NivARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NubvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NusvARB, callback);
    bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NuivARB, callback);
#endif
    bugle_register_filter_catches(f, CFUNC_glVertex2d, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex2dv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex2f, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex2fv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex2i, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex2iv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex2s, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex2sv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex3d, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex3dv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex3f, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex3fv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex3i, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex3iv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex3s, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex3sv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex4d, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex4dv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex4f, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex4fv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex4i, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex4iv, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex4s, callback);
    bugle_register_filter_catches(f, CFUNC_glVertex4sv, callback);

    bugle_register_filter_catches(f, CFUNC_glArrayElement, callback);
}

void bugle_register_filter_catches_drawing(filter *f, filter_callback callback)
{
    bugle_register_filter_catches_drawing_immediate(f, callback);

    bugle_register_filter_catches(f, CFUNC_glDrawElements, callback);
    bugle_register_filter_catches(f, CFUNC_glDrawArrays, callback);
#ifdef GL_EXT_draw_range_elements
    bugle_register_filter_catches(f, CFUNC_glDrawRangeElementsEXT, callback);
#endif
#ifdef GL_EXT_multi_draw_arrays
    bugle_register_filter_catches(f, CFUNC_glMultiDrawElementsEXT, callback);
    bugle_register_filter_catches(f, CFUNC_glMultiDrawArraysEXT, callback);
#endif
}

bool bugle_call_is_immediate(function_call *call)
{
    switch (bugle_canonical_call(call))
    {
#ifdef GL_ARB_vertex_program
    case CFUNC_glVertexAttrib1sARB:
    case CFUNC_glVertexAttrib1fARB:
    case CFUNC_glVertexAttrib1dARB:
    case CFUNC_glVertexAttrib2sARB:
    case CFUNC_glVertexAttrib2fARB:
    case CFUNC_glVertexAttrib2dARB:
    case CFUNC_glVertexAttrib3sARB:
    case CFUNC_glVertexAttrib3fARB:
    case CFUNC_glVertexAttrib3dARB:
    case CFUNC_glVertexAttrib4sARB:
    case CFUNC_glVertexAttrib4fARB:
    case CFUNC_glVertexAttrib4dARB:
    case CFUNC_glVertexAttrib4NubARB:
    case CFUNC_glVertexAttrib1svARB:
    case CFUNC_glVertexAttrib1fvARB:
    case CFUNC_glVertexAttrib1dvARB:
    case CFUNC_glVertexAttrib2svARB:
    case CFUNC_glVertexAttrib2fvARB:
    case CFUNC_glVertexAttrib2dvARB:
    case CFUNC_glVertexAttrib3svARB:
    case CFUNC_glVertexAttrib3fvARB:
    case CFUNC_glVertexAttrib3dvARB:
    case CFUNC_glVertexAttrib4bvARB:
    case CFUNC_glVertexAttrib4svARB:
    case CFUNC_glVertexAttrib4ivARB:
    case CFUNC_glVertexAttrib4ubvARB:
    case CFUNC_glVertexAttrib4usvARB:
    case CFUNC_glVertexAttrib4uivARB:
    case CFUNC_glVertexAttrib4fvARB:
    case CFUNC_glVertexAttrib4dvARB:
    case CFUNC_glVertexAttrib4NbvARB:
    case CFUNC_glVertexAttrib4NsvARB:
    case CFUNC_glVertexAttrib4NivARB:
    case CFUNC_glVertexAttrib4NubvARB:
    case CFUNC_glVertexAttrib4NusvARB:
    case CFUNC_glVertexAttrib4NuivARB:
        return (*(GLuint *) call->generic.args[0] == 0);
#endif
    case CFUNC_glVertex2d:
    case CFUNC_glVertex2dv:
    case CFUNC_glVertex2f:
    case CFUNC_glVertex2fv:
    case CFUNC_glVertex2i:
    case CFUNC_glVertex2iv:
    case CFUNC_glVertex2s:
    case CFUNC_glVertex2sv:
    case CFUNC_glVertex3d:
    case CFUNC_glVertex3dv:
    case CFUNC_glVertex3f:
    case CFUNC_glVertex3fv:
    case CFUNC_glVertex3i:
    case CFUNC_glVertex3iv:
    case CFUNC_glVertex3s:
    case CFUNC_glVertex3sv:
    case CFUNC_glVertex4d:
    case CFUNC_glVertex4dv:
    case CFUNC_glVertex4f:
    case CFUNC_glVertex4fv:
    case CFUNC_glVertex4i:
    case CFUNC_glVertex4iv:
    case CFUNC_glVertex4s:
    case CFUNC_glVertex4sv:
    case CFUNC_glArrayElement:
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
