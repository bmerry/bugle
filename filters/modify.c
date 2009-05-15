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
#include <stdlib.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <string.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glextensions.h>
#include <bugle/gl/gldisplaylist.h>
#include <bugle/linkedlist.h>
#include <bugle/hashtable.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/input.h>
#include <bugle/log.h>
#include <bugle/apireflect.h>
#include <budgie/reflect.h>
#include <budgie/addresses.h>

/*** Classify filter-set ***/
/* This is a helper filter-set that tries to guess whether the current
 * rendering is in 3D or not. Non-3D rendering includes building
 * non-graphical textures, depth-of-field and similar post-processing,
 * and deferred shading.
 *
 * The current heuristic is that FBOs are for non-3D and the window-system
 * framebuffer is for 3D. This is clearly less than perfect. Other possible
 * heuristics would involve transformation matrices (e.g. orthographic
 * projection matrix is most likely non-3D) and framebuffer and texture
 * sizes (drawing to a window-sized framebuffer is probably 3D, while
 * drawing from a window-sized framebuffer is probably non-3D).
 */

static object_view classify_view;
static linked_list classify_callbacks;

typedef struct
{
    bugle_bool real;
} classify_context;

typedef struct
{
    void (*callback)(bugle_bool, void *);
    void *arg;
} classify_callback;

static void register_classify_callback(void (*callback)(bugle_bool, void *), void *arg)
{
    classify_callback *cb;

    cb = BUGLE_MALLOC(classify_callback);
    cb->callback = callback;
    cb->arg = arg;
    bugle_list_append(&classify_callbacks, cb);
}

static void classify_context_init(const void *key, void *data)
{
    classify_context *ctx;

    ctx = (classify_context *) data;
    ctx->real = BUGLE_TRUE;
}

#ifdef GL_EXT_framebuffer_object
static bugle_bool classify_glBindFramebufferEXT(function_call *call, const callback_data *data)
{
    classify_context *ctx;
    GLint fbo;
    linked_list_node *i;
    classify_callback *cb;

    ctx = (classify_context *) bugle_object_get_current_data(bugle_context_class, classify_view);
    if (ctx && bugle_gl_begin_internal_render())
    {
#ifdef GL_EXT_framebuffer_blit
        if (BUGLE_GL_HAS_EXTENSION(GL_EXT_framebuffer_blit))
        {
            CALL(glGetIntegerv)(GL_DRAW_FRAMEBUFFER_BINDING_EXT, &fbo);
        }
        else
#endif
        {
            CALL(glGetIntegerv)(GL_FRAMEBUFFER_BINDING_EXT, &fbo);
        }
        ctx->real = (fbo == 0);
        bugle_gl_end_internal_render("classify_glBindFramebufferEXT", BUGLE_TRUE);
        for (i = bugle_list_head(&classify_callbacks); i; i = bugle_list_next(i))
        {
            cb = (classify_callback *) bugle_list_data(i);
            (*cb->callback)(ctx->real, cb->arg);
        }
    }
    return BUGLE_TRUE;
}
#endif

static bugle_bool classify_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "classify");
#ifdef GL_EXT_framebuffer_object
    bugle_filter_catches(f, "glBindFramebufferEXT", BUGLE_TRUE, classify_glBindFramebufferEXT);
#endif
    bugle_filter_order("invoke", "classify");
    bugle_gl_filter_post_renders("classify");
    classify_view = bugle_object_view_new(bugle_context_class,
                                          classify_context_init,
                                          NULL,
                                          sizeof(classify_context));
    bugle_list_init(&classify_callbacks, free);
    return BUGLE_TRUE;
}

static void classify_shutdown(filter_set *handle)
{
    bugle_list_clear(&classify_callbacks);
}

/*** Wireframe filter-set ***/

static filter_set *wireframe_filterset;
static object_view wireframe_view;

typedef struct
{
    bugle_bool active;            /* True if polygon mode has been adjusted */
    bugle_bool real;              /* True unless this is a render-to-X preprocess */
    GLint polygon_mode[2];  /* The app polygon mode, for front and back */
} wireframe_context;

static void wireframe_context_init(const void *key, void *data)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) data;
    ctx->active = BUGLE_FALSE;
    ctx->real = BUGLE_TRUE;
    ctx->polygon_mode[0] = GL_FILL;
    ctx->polygon_mode[1] = GL_FILL;

    if (bugle_filter_set_is_active(wireframe_filterset)
        && bugle_gl_begin_internal_render())
    {
        CALL(glPolygonMode)(GL_FRONT_AND_BACK, GL_LINE);
        ctx->real = BUGLE_TRUE;
        bugle_gl_end_internal_render("wireframe_context_init", BUGLE_TRUE);
    }
}

/* Handles on the fly activation and deactivation */
static void wireframe_handle_activation(bugle_bool active, wireframe_context *ctx)
{
    active &= ctx->real;
    if (active && !ctx->active)
    {
        if (bugle_gl_begin_internal_render())
        {
            CALL(glGetIntegerv)(GL_POLYGON_MODE, ctx->polygon_mode);
            CALL(glPolygonMode)(GL_FRONT_AND_BACK, GL_LINE);
            ctx->active = BUGLE_TRUE;
            bugle_gl_end_internal_render("wireframe_handle_activation", BUGLE_TRUE);
        }
    }
    else if (!active && ctx->active)
    {
        if (bugle_gl_begin_internal_render())
        {
            CALL(glPolygonMode)(GL_FRONT, ctx->polygon_mode[0]);
            CALL(glPolygonMode)(GL_BACK, ctx->polygon_mode[1]);
            ctx->active = BUGLE_FALSE;
            bugle_gl_end_internal_render("wireframe_handle_activation", BUGLE_TRUE);
        }
    }
}

/* This is called even when inactive, to restore the proper state to contexts
 * that were not current when activation or deactivation occurred.
 */
static bugle_bool wireframe_make_current(function_call *call, const callback_data *data)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(bugle_context_class, wireframe_view);
    if (ctx)
        wireframe_handle_activation(bugle_filter_set_is_active(data->filter_set_handle), ctx);
    return BUGLE_TRUE;
}

static bugle_bool wireframe_swap_buffers(function_call *call, const callback_data *data)
{
    /* apps that render the entire frame don't always bother to clear */
    CALL(glClear)(GL_COLOR_BUFFER_BIT);
    return BUGLE_TRUE;
}

static bugle_bool wireframe_glPolygonMode(function_call *call, const callback_data *data)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(bugle_context_class, wireframe_view);
    if (ctx && !ctx->real)
        return BUGLE_TRUE;
    if (bugle_gl_begin_internal_render())
    {
        if (ctx)
            CALL(glGetIntegerv)(GL_POLYGON_MODE, ctx->polygon_mode);
        CALL(glPolygonMode)(GL_FRONT_AND_BACK, GL_LINE);
        bugle_gl_end_internal_render("wireframe_glPolygonMode", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool wireframe_glEnable(function_call *call, const callback_data *data)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(bugle_context_class, wireframe_view);
    if (ctx && !ctx->real)
        return BUGLE_TRUE;
    /* FIXME: need to track this state to restore it on deactivation */
    /* FIXME: also need to nuke it at activation */
    /* FIXME: has no effect when using a fragment shader */
    switch (*call->glEnable.arg0)
    {
    case GL_TEXTURE_1D:
    case GL_TEXTURE_2D:
    case GL_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_3D:
        if (bugle_gl_begin_internal_render())
        {
            CALL(glDisable)(*call->glEnable.arg0);
            bugle_gl_end_internal_render("wireframe_callback", BUGLE_TRUE);
        }
    default: /* avoids compiler warning if GLenum is a C enum */ ;
    }
    return BUGLE_TRUE;
}

static void wireframe_classify_callback(bugle_bool real, void *arg)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(bugle_context_class, wireframe_view);
    if (ctx)
    {
        ctx->real = real;
        wireframe_handle_activation(bugle_filter_set_is_active((filter_set *) arg), ctx);
    }
}

static void wireframe_activation(filter_set *handle)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(bugle_context_class, wireframe_view);
    if (ctx)
        wireframe_handle_activation(BUGLE_TRUE, ctx);
}

static void wireframe_deactivation(filter_set *handle)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(bugle_context_class, wireframe_view);
    if (ctx)
        wireframe_handle_activation(BUGLE_FALSE, ctx);
}

static bugle_bool wireframe_initialise(filter_set *handle)
{
    filter *f;

    wireframe_filterset = handle;
    f = bugle_filter_new(handle, "wireframe");
    bugle_glwin_filter_catches_swap_buffers(f, BUGLE_FALSE, wireframe_swap_buffers);
    bugle_filter_catches(f, "glPolygonMode", BUGLE_FALSE, wireframe_glPolygonMode);
    bugle_filter_catches(f, "glEnable", BUGLE_FALSE, wireframe_glEnable);
    bugle_glwin_filter_catches_make_current(f, BUGLE_TRUE, wireframe_make_current);
    bugle_filter_order("invoke", "wireframe");
    bugle_gl_filter_post_renders("wireframe");
    wireframe_view = bugle_object_view_new(bugle_context_class,
                                           wireframe_context_init,
                                           NULL,
                                           sizeof(wireframe_context));
    register_classify_callback(wireframe_classify_callback, handle);
    return BUGLE_TRUE;
}

/*** Frontbuffer filterset */

static filter_set *frontbuffer_filterset;
static object_view frontbuffer_view;

typedef struct
{
    bugle_bool active;
    GLint draw_buffer;
} frontbuffer_context;

static void frontbuffer_context_init(const void *key, void *data)
{
    frontbuffer_context *ctx;

    ctx = (frontbuffer_context *) data;
    if (bugle_filter_set_is_active(frontbuffer_filterset)
        && bugle_gl_begin_internal_render())
    {
        ctx->active = BUGLE_TRUE;
        CALL(glGetIntegerv)(GL_DRAW_BUFFER, &ctx->draw_buffer);
        CALL(glDrawBuffer)(GL_FRONT);
        bugle_gl_end_internal_render("frontbuffer_context_init", BUGLE_TRUE);
    }
}

static void frontbuffer_handle_activation(bugle_bool active, frontbuffer_context *ctx)
{
    if (active && !ctx->active)
    {
        if (bugle_gl_begin_internal_render())
        {
            CALL(glGetIntegerv)(GL_DRAW_BUFFER, &ctx->draw_buffer);
            CALL(glDrawBuffer)(GL_FRONT);
            ctx->active = BUGLE_TRUE;
            bugle_gl_end_internal_render("frontbuffer_handle_activation", BUGLE_TRUE);
        }
    }
    else if (!active && ctx->active)
    {
        if (bugle_gl_begin_internal_render())
        {
            CALL(glDrawBuffer)(ctx->draw_buffer);
            ctx->active = BUGLE_FALSE;
            bugle_gl_end_internal_render("frontbuffer_handle_activation", BUGLE_TRUE);
        }
    }
}

static bugle_bool frontbuffer_make_current(function_call *call, const callback_data *data)
{
    frontbuffer_context *ctx;

    ctx = (frontbuffer_context *) bugle_object_get_current_data(bugle_context_class, frontbuffer_view);
    if (ctx)
        frontbuffer_handle_activation(bugle_filter_set_is_active(data->filter_set_handle), ctx);
    return BUGLE_TRUE;
}

static bugle_bool frontbuffer_glDrawBuffer(function_call *call, const callback_data *data)
{
    frontbuffer_context *ctx;

    if (bugle_gl_begin_internal_render())
    {
        ctx = (frontbuffer_context *) bugle_object_get_current_data(bugle_context_class, frontbuffer_view);
        if (ctx) CALL(glGetIntegerv)(GL_DRAW_BUFFER, &ctx->draw_buffer);
        CALL(glDrawBuffer)(GL_FRONT);
        bugle_gl_end_internal_render("frontbuffer_glDrawBuffer", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool frontbuffer_swap_buffers(function_call *call, const callback_data *data)
{
    /* swap_buffers is specified to do an implicit glFlush. We're going to
     * kill the call, so we need to do the flush explicitly.
     */
    CALL(glFlush)();
    return BUGLE_FALSE;
}

static void frontbuffer_activation(filter_set *handle)
{
    frontbuffer_context *ctx;

    ctx = (frontbuffer_context *) bugle_object_get_current_data(bugle_context_class, frontbuffer_view);
    if (ctx)
        frontbuffer_handle_activation(BUGLE_TRUE, ctx);
}

static void frontbuffer_deactivation(filter_set *handle)
{
    frontbuffer_context *ctx;

    ctx = (frontbuffer_context *) bugle_object_get_current_data(bugle_context_class, frontbuffer_view);
    if (ctx)
        frontbuffer_handle_activation(BUGLE_FALSE, ctx);
}

static bugle_bool frontbuffer_initialise(filter_set *handle)
{
    filter *f;

    frontbuffer_filterset = handle;
    f = bugle_filter_new(handle, "frontbuffer");
    bugle_filter_order("invoke", "frontbuffer");
    bugle_filter_catches(f, "glDrawBuffer", BUGLE_FALSE, frontbuffer_glDrawBuffer);
    bugle_glwin_filter_catches_make_current(f, BUGLE_TRUE, frontbuffer_make_current);
    bugle_gl_filter_post_renders("frontbuffer");

    f = bugle_filter_new(handle, "frontbuffer_pre");
    bugle_filter_order("frontbuffer_pre", "invoke");
    bugle_glwin_filter_catches_swap_buffers(f, BUGLE_FALSE, frontbuffer_swap_buffers);

    frontbuffer_view = bugle_object_view_new(bugle_context_class,
                                             frontbuffer_context_init,
                                             NULL,
                                             sizeof(frontbuffer_context));
    return BUGLE_TRUE;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info classify_info =
    {
        "classify",
        classify_initialise,
        classify_shutdown,
        NULL,
        NULL,
        NULL,
        NULL
    };
    static const filter_set_info wireframe_info =
    {
        "wireframe",
        wireframe_initialise,
        NULL,
        wireframe_activation,
        wireframe_deactivation,
        NULL,
        "renders in wireframe"
    };
    static const filter_set_info frontbuffer_info =
    {
        "frontbuffer",
        frontbuffer_initialise,
        NULL,
        frontbuffer_activation,
        frontbuffer_deactivation,
        NULL,
        "renders directly to the frontbuffer (can see scene being built)"
    };

    bugle_filter_set_new(&classify_info);
    bugle_filter_set_new(&wireframe_info);
    bugle_filter_set_new(&frontbuffer_info);

    bugle_gl_filter_set_renders("classify");
    bugle_filter_set_depends("classify", "trackcontext");
    bugle_filter_set_depends("classify", "glextensions");
    bugle_gl_filter_set_renders("wireframe");
    bugle_filter_set_depends("wireframe", "trackcontext");
    bugle_filter_set_depends("wireframe", "classify");
    bugle_gl_filter_set_renders("frontbuffer");
}
