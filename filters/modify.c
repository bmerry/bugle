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
#include "src/canon.h"
#include "src/glutils.h"
#include "src/objects.h"
#include "src/tracker.h"
#include "common/bool.h"
#include <GL/glx.h>
#include <sys/time.h>

/* Wireframe filter-set */

static void initialise_wireframe_context(const void *key, void *data)
{
    if (bugle_begin_internal_render())
    {
        CALL_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        bugle_end_internal_render("initialise_wireframe_context", true);
    }
}

static bool wireframe_callback(function_call *call, const callback_data *data)
{
    switch (bugle_canonical_call(call))
    {
    case CFUNC_glXSwapBuffers:
        CALL_glClear(GL_COLOR_BUFFER_BIT); /* hopefully bypass z-trick */
        break;
    case CFUNC_glPolygonMode:
        if (bugle_begin_internal_render())
        {
            CALL_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            bugle_end_internal_render("wireframe_callback", true);
        }
        break;
    case CFUNC_glEnable:
        switch (*call->typed.glEnable.arg0)
        {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
#ifdef GL_ARB_texture_cube_map
        case GL_TEXTURE_CUBE_MAP_ARB:
#endif
#ifdef GL_EXT_texture3D
        case GL_TEXTURE_3D_EXT:
#endif
            if (bugle_begin_internal_render())
            {
                CALL_glDisable(*call->typed.glEnable.arg0);
                bugle_end_internal_render("wireframe_callback", true);
            }
            break;
        default: ;
        }
    }
    return true;
}

static bool initialise_wireframe(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "wireframe", wireframe_callback);
    bugle_register_filter_catches(f, CFUNC_glPolygonMode);
    bugle_register_filter_catches(f, CFUNC_glEnable);
    bugle_register_filter_depends("wireframe", "invoke");
    bugle_register_filter_post_renders("wireframe");
    bugle_object_class_register(&bugle_context_class, initialise_wireframe_context,
                                NULL, 0);
    return true;
}

static void initialise_frontbuffer_context(const void *key, void *data)
{
    if (bugle_begin_internal_render())
    {
        CALL_glDrawBuffer(GL_FRONT);
        bugle_end_internal_render("initialise_frontbuffer_context", true);
    }
}

static bool frontbuffer_callback(function_call *call, const callback_data *data)
{
    switch (bugle_canonical_call(call))
    {
    case CFUNC_glDrawBuffer:
        if (bugle_begin_internal_render())
        {
            CALL_glDrawBuffer(GL_FRONT);
            bugle_end_internal_render("frontbuffer_callback", true);
        }
        break;
    }
    return true;
}

static bool initialise_frontbuffer(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "frontbuffer", frontbuffer_callback);
    bugle_register_filter_depends("frontbuffer", "invoke");
    bugle_register_filter_catches(f, CFUNC_glDrawBuffer);
    bugle_register_filter_set_renders("frontbuffer");
    bugle_register_filter_post_renders("frontbuffer");
    bugle_object_class_register(&bugle_context_class, initialise_frontbuffer_context,
                                NULL, 0);
    return true;
}

void bugle_initialise_filter_library(void)
{
    const filter_set_info wireframe_info =
    {
        "wireframe",
        initialise_wireframe,
        NULL,
        NULL,
        0
    };
    const filter_set_info frontbuffer_info =
    {
        "frontbuffer",
        initialise_frontbuffer,
        NULL,
        NULL,
        0
    };
    bugle_register_filter_set(&wireframe_info);
    bugle_register_filter_set(&frontbuffer_info);

    bugle_register_filter_set_renders("wireframe");
}
