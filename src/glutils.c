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
#include "tracker.h"
#include "src/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <assert.h>

static filter_set *error_handle = NULL;

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
            dump_GLerror(error, stderr);
            fputs(".\n", stderr);
        }
    }
}

GLXContext get_aux_context()
{
    state_7context_I *context_state;
    GLXContext old_ctx, ctx;
    int render_type = 0, screen;
    int n;
    int attribs[3] = {GLX_FBCONFIG_ID, 0, None};
    GLXFBConfig *cfgs;
    Display *dpy;

    context_state = tracker_get_context_state();
    if (context_state->c_internal.c_auxcontext.data == NULL)
    {
        dpy = glXGetCurrentDisplay();
        old_ctx = glXGetCurrentContext();
        glXQueryContext(dpy, old_ctx, GLX_RENDER_TYPE, &render_type);
        glXQueryContext(dpy, old_ctx, GLX_SCREEN, &screen);
        glXQueryContext(dpy, old_ctx, GLX_FBCONFIG_ID, &attribs[1]);
        /* It seems that render_type comes back as a boolean, although
         * the spec seems to indicate that it should be otherwise.
         */
        if (render_type <= 1)
            render_type = render_type ? GLX_RGBA_TYPE : GLX_COLOR_INDEX_TYPE;
        cfgs = glXChooseFBConfig(dpy, screen, attribs, &n);
        if (cfgs == NULL)
        {
            fprintf(stderr, "Warning: could not create an auxiliary context\n");
            return NULL;
        }
        ctx = CALL_glXCreateNewContext(dpy, cfgs[0], render_type,
                                       old_ctx, glXIsDirect(dpy, old_ctx));
        XFree(cfgs);
        if (ctx == NULL)
            fprintf(stderr, "Warning: could not create an auxiliary context\n");
        context_state->c_internal.c_auxcontext.data = ctx;
    }
    return context_state->c_internal.c_auxcontext.data;
}

void register_filter_set_renders(const char *name)
{
    register_filter_set_uses_state(name);
    register_filter_set_depends(name, "trackbeginend");
}

void register_filter_post_renders(const char *name)
{
    register_filter_depends(name, "error");
    register_filter_depends(name, "trackbeginend");
}

void register_filter_set_uses_state(const char *name)
{
    register_filter_set_depends(name, "trackcontext");
}

void register_filter_post_uses_state(const char *name)
{
    register_filter_depends(name, "trackcontext");
}

void register_filter_set_queries_error(const char *name, bool require)
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

bool gl_has_extension(const char *name)
{
    const char *exts;
    size_t len;

    exts = (const char *) CALL_glGetString(GL_EXTENSIONS);
    len = strlen(name);
    while ((exts = strstr(exts, name)) != NULL)
    {
        if (exts[len] == '\0' || exts[len] == ' ') return true;
    }
    return false;
}
