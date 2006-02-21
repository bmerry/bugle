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

#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/objects.h"
#include "src/tracker.h"
#include "common/bool.h"
#include <GL/glx.h>
#include <sys/time.h>

/*** Wireframe filter-set ***/

static filter_set *wireframe_filterset;
static bugle_object_view wireframe_view;

typedef struct
{
    bool active; /* True if polygon mode has been adjusted */
    GLenum polygon_mode[2];  /* The app polygon mode, for front and back */
} wireframe_context;

static void initialise_wireframe_context(const void *key, void *data)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) data;
    ctx->active = false;
    ctx->polygon_mode[0] = GL_FILL;
    ctx->polygon_mode[1] = GL_FILL;

    if (bugle_filter_set_is_active(wireframe_filterset)
        && bugle_begin_internal_render())
    {
        CALL_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        ctx->active = true;
        bugle_end_internal_render("initialise_wireframe_context", true);
    }
}

/* Handles on the fly activation and deactivation */
static void wireframe_handle_activation(bool active, wireframe_context *ctx)
{
    if (active && !ctx->active)
    {
        if (bugle_begin_internal_render())
        {
            CALL_glGetIntegerv(GL_POLYGON_MODE, ctx->polygon_mode);
            CALL_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            ctx->active = true;
            bugle_end_internal_render("wireframe_handle_activation", true);
        }
    }
    else if (!active && ctx->active)
    {
        if (bugle_begin_internal_render())
        {
            CALL_glPolygonMode(GL_FRONT, ctx->polygon_mode[0]);
            CALL_glPolygonMode(GL_BACK, ctx->polygon_mode[1]);
            ctx->active = false;
            bugle_end_internal_render("wireframe_handle_activation", true);
        }
    }
}

/* Handles both glXMakeCurrent and glXMakeContextCurrent. It is called even
 * when inactive, to restore the proper state to contexts that were not
 * current when activation or deactivation occurred.
 */
static bool wireframe_make_current(function_call *call, const callback_data *data)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(&bugle_context_class, wireframe_view);
    if (ctx)
        wireframe_handle_activation(bugle_filter_set_is_active(data->filter_set_handle), ctx);
    return true;
}

static bool wireframe_glXSwapBuffers(function_call *call, const callback_data *data)
{
    CALL_glClear(GL_COLOR_BUFFER_BIT); /* hopefully bypass z-trick */
    return true;
}

static bool wireframe_glPolygonMode(function_call *call, const callback_data *data)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(&bugle_context_class, wireframe_view);
    if (bugle_begin_internal_render())
    {
        if (ctx)
            CALL_glGetIntegerv(GL_POLYGON_MODE, ctx->polygon_mode);
        CALL_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        bugle_end_internal_render("wireframe_glPolygonMode", true);
    }
    return true;
}

static bool wireframe_glEnable(function_call *call, const callback_data *data)
{
    /* FIXME: need to track this state to restore it on deactivation */
    /* FIXME: also need to nuke it at activation */
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
    default: /* avoids compiler warning if GLenum is a C enum */ ;
    }
    return true;
}

static void wireframe_activation(filter_set *handle)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(&bugle_context_class, wireframe_view);
    if (ctx)
        wireframe_handle_activation(true, ctx);
}

static void wireframe_deactivation(filter_set *handle)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(&bugle_context_class, wireframe_view);
    if (ctx)
        wireframe_handle_activation(false, ctx);
}

static bool initialise_wireframe(filter_set *handle)
{
    filter *f;

    wireframe_filterset = handle;
    f = bugle_register_filter(handle, "wireframe");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, wireframe_glXSwapBuffers);
    bugle_register_filter_catches(f, GROUP_glPolygonMode, false, wireframe_glPolygonMode);
    bugle_register_filter_catches(f, GROUP_glEnable, false, wireframe_glEnable);
    bugle_register_filter_catches(f, GROUP_glXMakeCurrent, true, wireframe_make_current);
#ifdef GLX_VERSION_1_3
    bugle_register_filter_catches(f, GROUP_glXMakeContextCurrent, true, wireframe_make_current);
#endif
    bugle_register_filter_depends("wireframe", "invoke");
    bugle_register_filter_post_renders("wireframe");
    wireframe_view = bugle_object_class_register(&bugle_context_class, initialise_wireframe_context,
                                                 NULL, sizeof(wireframe_context));
    return true;
}

/*** Frontbuffer filterset */

static filter_set *frontbuffer_filterset;
static bugle_object_view frontbuffer_view;

typedef struct
{
    bool active;
    GLenum draw_buffer;
} frontbuffer_context;

static void initialise_frontbuffer_context(const void *key, void *data)
{
    frontbuffer_context *ctx;

    ctx = (frontbuffer_context *) data;
    if (bugle_filter_set_is_active(frontbuffer_filterset)
        && bugle_begin_internal_render())
    {
        ctx->active = true;
        CALL_glGetIntegerv(GL_DRAW_BUFFER, &ctx->draw_buffer);
        CALL_glDrawBuffer(GL_FRONT);
        bugle_end_internal_render("initialise_frontbuffer_context", true);
    }
}

static void frontbuffer_handle_activation(bool active, frontbuffer_context *ctx)
{
    if (active && !ctx->active)
    {
        if (bugle_begin_internal_render())
        {
            CALL_glGetIntegerv(GL_DRAW_BUFFER, &ctx->draw_buffer);
            CALL_glDrawBuffer(GL_FRONT);
            ctx->active = true;
            bugle_end_internal_render("frontbuffer_handle_activation", true);
        }
    }
    else if (!active && ctx->active)
    {
        if (bugle_begin_internal_render())
        {
            CALL_glDrawBuffer(ctx->draw_buffer);
            ctx->active = false;
            bugle_end_internal_render("frontbuffer_handle_activation", true);
        }
    }
}

static bool frontbuffer_make_current(function_call *call, const callback_data *data)
{
    frontbuffer_context *ctx;

    ctx = (frontbuffer_context *) bugle_object_get_current_data(&bugle_context_class, frontbuffer_view);
    if (ctx)
        frontbuffer_handle_activation(bugle_filter_set_is_active(data->filter_set_handle), ctx);
    return true;
}

static bool frontbuffer_glDrawBuffer(function_call *call, const callback_data *data)
{
    frontbuffer_context *ctx;

    if (bugle_begin_internal_render())
    {
        ctx = (frontbuffer_context *) bugle_object_get_current_data(&bugle_context_class, frontbuffer_view);
        if (ctx) CALL_glGetIntegerv(GL_DRAW_BUFFER, &ctx->draw_buffer);
        CALL_glDrawBuffer(GL_FRONT);
        bugle_end_internal_render("frontbuffer_glDrawBuffer", true);
    }
    return true;
}

static bool frontbuffer_glXSwapBuffers(function_call *call, const callback_data *data)
{
    /* glXSwapBuffers is specified to do an implicit glFlush. We're going to
     * kill the call, so we need to do the flush explicitly.
     */
    CALL_glFlush();
    return false;
}

static void frontbuffer_activation(filter_set *handle)
{
    frontbuffer_context *ctx;

    ctx = (frontbuffer_context *) bugle_object_get_current_data(&bugle_context_class, frontbuffer_view);
    if (ctx)
        frontbuffer_handle_activation(true, ctx);
}

static void frontbuffer_deactivation(filter_set *handle)
{
    frontbuffer_context *ctx;

    ctx = (frontbuffer_context *) bugle_object_get_current_data(&bugle_context_class, frontbuffer_view);
    if (ctx)
        frontbuffer_handle_activation(false, ctx);
}

static bool initialise_frontbuffer(filter_set *handle)
{
    filter *f;

    frontbuffer_filterset = handle;
    f = bugle_register_filter(handle, "frontbuffer");
    bugle_register_filter_depends("frontbuffer", "invoke");
    bugle_register_filter_catches(f, GROUP_glDrawBuffer, false, frontbuffer_glDrawBuffer);
    bugle_register_filter_catches(f, GROUP_glXMakeCurrent, true, frontbuffer_make_current);
#ifdef GLX_VERSION_1_3
    bugle_register_filter_catches(f, GROUP_glXMakeContextCurrent, true, frontbuffer_make_current);
#endif
    bugle_register_filter_post_renders("frontbuffer");

    f = bugle_register_filter(handle, "frontbuffer_pre");
    bugle_register_filter_depends("invoke", "frontbuffer_pre");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, frontbuffer_glXSwapBuffers);

    frontbuffer_view = bugle_object_class_register(&bugle_context_class, initialise_frontbuffer_context,
                                                   NULL, sizeof(frontbuffer_context));
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info wireframe_info =
    {
        "wireframe",
        initialise_wireframe,
        NULL,
        wireframe_activation,
        wireframe_deactivation,
        NULL,
        0,
        "renders in wireframe"
    };
    static const filter_set_info frontbuffer_info =
    {
        "frontbuffer",
        initialise_frontbuffer,
        NULL,
        frontbuffer_activation,
        frontbuffer_deactivation,
        NULL,
        0,
        "renders directly to the frontbuffer (can see scene being built)"
    };
    bugle_register_filter_set(&wireframe_info);
    bugle_register_filter_set(&frontbuffer_info);

    bugle_register_filter_set_renders("wireframe");
    bugle_register_filter_set_renders("frontbuffer");
}
