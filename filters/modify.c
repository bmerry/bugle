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

#include "src/filters.h"
#include "src/utils.h"
#include "src/types.h"
#include "src/canon.h"
#include "src/glutils.h"
#include "common/bool.h"
#include <GL/glx.h>

/* Wireframe filter-set */

static bool wireframe_callback(function_call *call, void *data)
{
    switch (canonical_call(call))
    {
    case CFUNC_glPolygonMode:
    case CFUNC_glXMakeCurrent:
#ifdef CFUNC_glXMakeContextCurrent
    case CFUNC_glXMakeContextCurrent:
#endif
    case CFUNC_glXSwapBuffers:
        if (begin_internal_render())
        {
            CALL_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            end_internal_render("wireframe", true);
        }
        break;
    case CFUNC_glEnable:
        switch (*call->typed.glEnable.arg0)
        {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
#ifdef GL_ARB_texture_cube_map
        case GL_TEXTURE_CUBE_MAP:
#endif
#ifdef GL_EXT_texture3D
        case GL_TEXTURE_3D:
#endif
            if (begin_internal_render())
            {
                CALL_glDisable(*call->typed.glEnable.arg0);
                end_internal_render("wireframe", true);
            }
            break;
        default: ;
        }
    }
    return true;
}

static bool initialise_wireframe(filter_set *handle)
{
    register_filter(handle, "wireframe", wireframe_callback);
    register_filter_depends("wireframe", "invoke");
    filter_set_renders("wireframe");
    filter_post_renders("wireframe");
    return true;
}

static bool frontbuffer_callback(function_call *call, void *data)
{
    switch (canonical_call(call))
    {
#ifdef CFUNC_glXMakeContextCurrent
    case CFUNC_glXMakeContextCurrent:
#endif
    case CFUNC_glXMakeCurrent:
    case CFUNC_glDrawBuffer:
        begin_internal_render();
        CALL_glDrawBuffer(GL_FRONT);
        CALL_glClear(GL_COLOR_BUFFER_BIT); /* hopefully bypass z-trick */
        end_internal_render("frontbuffer", true);
    }
    return true;
}

static bool initialise_frontbuffer(filter_set *handle)
{
    register_filter(handle, "frontbuffer", frontbuffer_callback);
    register_filter_depends("frontbuffer", "invoke");
    return true;
}

void initialise_filter_library(void)
{
    register_filter_set("wireframe", initialise_wireframe, NULL, NULL);
    register_filter_set("frontbuffer", initialise_frontbuffer, NULL, NULL);
}
