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
#include "common/bool.h"

/* Wireframe filter-set */

static bool wireframe_pre_callback(function_call *call, void *data)
{
    switch (canonical_call(call))
    {
    case FUNC_glEnable:
        switch (*call->typed.glEnable.arg0)
        {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_3D:
            return false;
        }
        break;
    case FUNC_glPolygonMode:
        return false;
    }
    return true;
}

static bool wireframe_post_callback(function_call *call, void *data)
{
    switch (canonical_call(call))
    {
    case FUNC_glXMakeCurrent:
    case FUNC_glXMakeContextCurrent:
        (*glPolygonMode_real)(GL_FRONT_AND_BACK, GL_LINE);
    }
    return true;
}

static bool initialise_wireframe(filter_set *handle)
{
    register_filter(handle, "wireframe_pre", wireframe_pre_callback);
    register_filter(handle, "wireframe_post", wireframe_post_callback);
    register_filter_depends("invoke", "wireframe_pre");
    register_filter_depends("wireframe_post", "invoke");
    return true;
}

static bool frontbuffer_callback(function_call *call, void *data)
{
    switch (canonical_call(call))
    {
    case FUNC_glXMakeCurrent:
    case FUNC_glXMakeContextCurrent:
    case FUNC_glDrawBuffer:
        (*glDrawBuffer_real)(GL_FRONT);
        (*glClear_real)(GL_COLOR_BUFFER_BIT); /* hopefully bypass z-trick */
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
