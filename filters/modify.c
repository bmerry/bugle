/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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

#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/objects.h"
#include "src/tracker.h"
#include "src/xevent.h"
#include "common/bool.h"
#include <GL/glx.h>
#include <sys/time.h>
#include <math.h>

/*** Wireframe filter-set ***/

static filter_set *wireframe_filterset;
static bugle_object_view wireframe_view;

typedef struct
{
    bool active; /* True if polygon mode has been adjusted */
    GLint polygon_mode[2];  /* The app polygon mode, for front and back */
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
    GLint draw_buffer;
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

/*** Camera filterset ***/

#define CAMERA_KEY_FORWARD 0
#define CAMERA_KEY_BACK 1
#define CAMERA_KEY_LEFT 2
#define CAMERA_KEY_RIGHT 3
#define CAMERA_MOTION_KEYS 4

#define CAMERA_KEY_FASTER 4
#define CAMERA_KEY_SLOWER 5
#define CAMERA_KEY_RESET 6
#define CAMERA_KEY_TOGGLE 7
#define CAMERA_KEYS 8

static filter_set *camera_filterset;
static bugle_object_view camera_view;
static float camera_speed = 1.0f;
static bool camera_dga = false;
static bool camera_intercept = true;

static xevent_key key_camera[CAMERA_KEYS] =
{
    { XK_Up, 0, true },
    { XK_Down, 0, true },
    { XK_Left, 0, true },
    { XK_Right, 0, true },
    { XK_Page_Up, 0, true },
    { XK_Page_Down, 0, true },
    { NoSymbol, 0, true }
};

typedef struct
{
    GLfloat original[16];
    GLfloat modifier[16];
    bool active;
    bool dirty;       /* Set to true when mouse moves */
    bool pressed[CAMERA_MOTION_KEYS];
} camera_context;

static void invalidate_window(XEvent *event)
{
    Display *dpy;
    XEvent dirty;
    Window w;
    Window root;
    int x, y;
    unsigned int width, height, border_width, depth;

    dpy = event->xany.display;
    w = event->xany.window != None ? event->xany.window : PointerWindow;
    dirty.type = Expose;
    dirty.xexpose.display = dpy;
    dirty.xexpose.window = event->xany.window;
    dirty.xexpose.x = 0;
    dirty.xexpose.y = 0;
    dirty.xexpose.width = 1;
    dirty.xexpose.height = 1;
    dirty.xexpose.count = 0;
    if (event->xany.window != None
        && XGetGeometry(dpy, w, &root, &x, &y, &width, &height, &border_width, &depth))
    {
        dirty.xexpose.width = width;
        dirty.xexpose.height = height;
    }
    XSendEvent(dpy, PointerWindow, True, ExposureMask, &dirty);
}

static void camera_mouse_callback(int dx, int dy, XEvent *event)
{
    camera_context *ctx;
    GLfloat old[16], rotator[3][3];
    GLfloat axis[3];
    GLfloat angle, cs, sn;
    int i, j, k;

    ctx = (camera_context *) bugle_object_get_current_data(&bugle_context_class, camera_view);
    if (!ctx) return;

    /* We use a rolling-ball type rotation. We do the calculations
     * ourselves rather than farming them out to OpenGL, since this
     * event could be processed asynchronously and OpenGL might not
     * be in a suitable state.
     */
    axis[0] = dy;
    axis[1] = dx;
    axis[2] = 0.0f;
    angle = sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    axis[0] /= angle;
    axis[1] /= angle;
    axis[2] /= angle;
    angle *= 0.005f;

    memcpy(old, ctx->modifier, sizeof(old));
    cs = cosf(angle);
    sn = sinf(angle);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
        {
            rotator[i][j] = (1 - cs) * axis[i] * axis[j];
            if (i == j) rotator[i][j] += cs;
            else if ((i + 1) % 3 == j)
                rotator[i][j] -= sn * axis[3 - i - j];
            else
                rotator[i][j] += sn * axis[3 - i - j];
        }
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
        {
            ctx->modifier[j * 4 + i] = 0.0f;
            for (k = 0; k < 3; k++)
                ctx->modifier[j * 4 + i] += rotator[i][k] * old[j * 4 + k];
        }
    ctx->dirty = true;

    invalidate_window(event);
}

static bool camera_key_wanted(const xevent_key *key, void *arg, XEvent *event)
{
    return bugle_filter_set_is_active(camera_filterset);
}

static void camera_key_callback(const xevent_key *key, void *arg, XEvent *event)
{
    int index, i;
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(&bugle_context_class, camera_view);
    if (!ctx) return;
    index = (xevent_key *) arg - key_camera;
    if (index < CAMERA_MOTION_KEYS)
    {
        ctx->pressed[index] = key->press;
        if (key->press) invalidate_window(event);
    }
    else
    {
        switch (index)
        {
        case CAMERA_KEY_FASTER: camera_speed *= 2.0f; break;
        case CAMERA_KEY_SLOWER: camera_speed *= 0.5f; break;
        case CAMERA_KEY_RESET:
            for (i = 0; i < 16; i++)
                ctx->modifier[i] = (i % 5) ? 0.0f : 1.0f;
            invalidate_window(event);
            break;
        case CAMERA_KEY_TOGGLE:
            if (!camera_intercept)
            {
                camera_intercept = true;
                bugle_xevent_grab_pointer(camera_dga, camera_mouse_callback);
                break;
            }
            else
            {
                camera_intercept = false;
                bugle_xevent_release_pointer();
            }
            break;
        }
    }
}

static void initialise_camera_context(const void *key, void *data)
{
    camera_context *ctx;
    int i;

    ctx = (camera_context *) data;
    for (i = 0; i < 4; i++)
        ctx->modifier[i * 5] = 1.0f;

    if (bugle_filter_set_is_active(camera_filterset)
        && bugle_begin_internal_render())
    {
        ctx->active = true;
        CALL_glGetFloatv(GL_MODELVIEW_MATRIX, ctx->original);
        bugle_end_internal_render("initialise_camera_context", true);
    }
}

static bool camera_restore(function_call *call, const callback_data *data)
{
    GLint mode;
    camera_context *ctx;

    if (bugle_displaylist_mode() == GL_NONE)
    {
        ctx = (camera_context *) bugle_object_get_current_data(&bugle_context_class, camera_view);
        if (ctx && bugle_begin_internal_render())
        {
            CALL_glGetIntegerv(GL_MATRIX_MODE, &mode);
            if (mode == GL_MODELVIEW)
                CALL_glLoadMatrixf(ctx->original);
            bugle_end_internal_render("camera_restore", true);
        }
    }
    return true;
}

static bool camera_override(function_call *call, const callback_data *data)
{
    GLint mode;
    camera_context *ctx;

    if (bugle_displaylist_mode() == GL_NONE)
    {
        ctx = (camera_context *) bugle_object_get_current_data(&bugle_context_class, camera_view);
        if (ctx && bugle_begin_internal_render())
        {
            CALL_glGetIntegerv(GL_MATRIX_MODE, &mode);
            if (mode == GL_MODELVIEW)
            {
                CALL_glGetFloatv(GL_MODELVIEW_MATRIX, ctx->original);
                CALL_glLoadMatrixf(ctx->modifier);
                CALL_glMultMatrixf(ctx->original);
            }
            bugle_end_internal_render("camera_restore", true);
        }
    }
    return true;
}

static bool camera_get(function_call *call, const callback_data *data)
{
    int i;
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(&bugle_context_class, camera_view);
    if (!ctx) return true;

    switch (call->generic.group)
    {
    case GROUP_glGetFloatv:
        for (i = 0; i < 16; i++)
            (*call->typed.glGetFloatv.arg1)[i] = ctx->original[i];
        break;
    case GROUP_glGetDoublev:
        for (i = 0; i < 16; i++)
            (*call->typed.glGetDoublev.arg1)[i] = ctx->original[i];
        break;
    }
    return true;
}

static bool camera_glXSwapBuffers(function_call *call, const callback_data *data)
{
    camera_context *ctx;
    GLint mode;

    ctx = (camera_context *) bugle_object_get_current_data(&bugle_context_class, camera_view);
    if (ctx && bugle_begin_internal_render())
    {
        int f = 0, l = 0;
        if (ctx->pressed[CAMERA_KEY_FORWARD]) f++;
        if (ctx->pressed[CAMERA_KEY_BACK]) f--;
        if (ctx->pressed[CAMERA_KEY_LEFT]) l++;
        if (ctx->pressed[CAMERA_KEY_RIGHT]) l--;
        if (f || l || ctx->dirty)
        {
            ctx->modifier[14] += f * camera_speed;
            ctx->modifier[12] += l * camera_speed;
            CALL_glGetIntegerv(GL_MATRIX_MODE, &mode);
            CALL_glMatrixMode(GL_MODELVIEW);
            CALL_glLoadMatrixf(ctx->modifier);
            CALL_glMultMatrixf(ctx->original);
            CALL_glMatrixMode(mode);
            ctx->dirty = false;
        }
        bugle_end_internal_render("camera_glXSwapBuffers", true);
    }
    return true;
}

static void camera_handle_activation(bool active, camera_context *ctx)
{
    GLint mode;

    if (active && !ctx->active)
    {
        if (bugle_begin_internal_render())
        {
            CALL_glGetFloatv(GL_MODELVIEW_MATRIX, ctx->original);
            ctx->active = true;
            ctx->dirty = true;
            bugle_end_internal_render("camera_handle_activation", true);
        }
    }
    else if (!active && ctx->active)
    {
        if (bugle_begin_internal_render())
        {
            CALL_glGetIntegerv(GL_MATRIX_MODE, &mode);
            CALL_glMatrixMode(GL_MODELVIEW);
            CALL_glLoadMatrixf(ctx->original);
            CALL_glMatrixMode(mode);
            ctx->active = false;
            bugle_end_internal_render("camera_handle_activation", true);
        }
    }
}

static bool camera_make_current(function_call *call, const callback_data *data)
{
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(&bugle_context_class, camera_view);
    if (ctx)
        camera_handle_activation(bugle_filter_set_is_active(data->filter_set_handle), ctx);
    return true;
}

static void camera_activation(filter_set *handle)
{
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(&bugle_context_class, camera_view);
    if (ctx)
        camera_handle_activation(true, ctx);
    if (camera_intercept)
        bugle_xevent_grab_pointer(camera_dga, camera_mouse_callback);
}

static void camera_deactivation(filter_set *handle)
{
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(&bugle_context_class, camera_view);
    if (ctx)
        camera_handle_activation(false, ctx);
    if (camera_intercept)
        bugle_xevent_release_pointer();
}

static bool initialise_camera(filter_set *handle)
{
    filter *f;
    int i;

    camera_filterset = handle;
    f = bugle_register_filter(handle, "camera_pre");
    bugle_register_filter_depends("invoke", "camera_pre");
    bugle_register_filter_catches(f, GROUP_glMultMatrixf, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glMultMatrixd, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glTranslatef, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glTranslated, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glScalef, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glScaled, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glRotatef, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glRotated, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glFrustum, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glPushMatrix, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glPopMatrix, false, camera_restore);
#ifdef GL_ARB_transpose_matrix
    bugle_register_filter_catches(f, GROUP_glMultTransposeMatrixf, false, camera_restore);
    bugle_register_filter_catches(f, GROUP_glMultTransposeMatrixd, false, camera_restore);
#endif

    f = bugle_register_filter(handle, "camera_post");
    bugle_register_filter_post_renders("camera_post");
    bugle_register_filter_depends("camera_post", "invoke");
    bugle_register_filter_catches(f, GROUP_glXMakeCurrent, true, camera_make_current);
#ifdef GLX_VERSION_1_3
    bugle_register_filter_catches(f, GROUP_glXMakeContextCurrent, true, camera_make_current);
#endif
    bugle_register_filter_catches(f, GROUP_glLoadIdentity, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glLoadMatrixf, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glLoadMatrixd, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glMultMatrixf, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glMultMatrixd, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glTranslatef, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glTranslated, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glScalef, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glScaled, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glRotatef, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glRotated, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glFrustum, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glPushMatrix, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glPopMatrix, false, camera_override);
#ifdef GL_ARB_transpose_matrix
    bugle_register_filter_catches(f, GROUP_glLoadTransposeMatrixf, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glLoadTransposeMatrixd, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glMultTransposeMatrixf, false, camera_override);
    bugle_register_filter_catches(f, GROUP_glMultTransposeMatrixd, false, camera_override);
#endif
    bugle_register_filter_catches(f, GROUP_glGetFloatv, false, camera_get);
    bugle_register_filter_catches(f, GROUP_glGetDoublev, false, camera_get);
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, camera_glXSwapBuffers);

    camera_view = bugle_object_class_register(&bugle_context_class, initialise_camera_context,
                                              NULL, sizeof(camera_context));

    for (i = 0; i < CAMERA_KEYS; i++)
    {
        xevent_key release;

        release = key_camera[i];
        release.press = false;
        bugle_register_xevent_key(&key_camera[i], camera_key_wanted, camera_key_callback, (void *) &key_camera[i]);
        if (i < CAMERA_MOTION_KEYS)
            bugle_register_xevent_key(&release, camera_key_wanted, camera_key_callback, (void *) &key_camera[i]);
    }
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
    static const filter_set_variable_info camera_variables[] =
    {
        { "mouse_dga", "mouse is controlled with XF86DGA extension", FILTER_SET_VARIABLE_BOOL, &camera_dga, NULL },
        { "key_forward", "key to move forward [Up]", FILTER_SET_VARIABLE_KEY, &key_camera[CAMERA_KEY_FORWARD], NULL },
        { "key_back", "key to move back [Down]", FILTER_SET_VARIABLE_KEY, &key_camera[CAMERA_KEY_BACK], NULL },
        { "key_left", "key to move left [Left]", FILTER_SET_VARIABLE_KEY, &key_camera[CAMERA_KEY_LEFT], NULL },
        { "key_right", "key to move right [Right]", FILTER_SET_VARIABLE_KEY, &key_camera[CAMERA_KEY_RIGHT], NULL },
        { "key_faster", "key to double camera speed [PgUp]", FILTER_SET_VARIABLE_KEY, &key_camera[CAMERA_KEY_FASTER], NULL },
        { "key_slower", "key to halve camera speed [PgDn]", FILTER_SET_VARIABLE_KEY, &key_camera[CAMERA_KEY_SLOWER], NULL },
        { "key_reset", "key to undo adjustments [none]", FILTER_SET_VARIABLE_KEY, &key_camera[CAMERA_KEY_RESET], NULL },
        { "key_toggle", "key to toggle mouse grab [none]", FILTER_SET_VARIABLE_KEY, &key_camera[CAMERA_KEY_TOGGLE], NULL },
        { "speed", "initial speed of camera [1.0]", FILTER_SET_VARIABLE_FLOAT, &camera_speed, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info camera_info =
    {
        "camera",
        initialise_camera,
        NULL,
        camera_activation,
        camera_deactivation,
        camera_variables,
        0,
        "allows the camera position to be changed on the fly"
    };

    bugle_register_filter_set(&wireframe_info);
    bugle_register_filter_set(&frontbuffer_info);
    bugle_register_filter_set(&camera_info);

    bugle_register_filter_set_renders("wireframe");
    bugle_register_filter_set_renders("frontbuffer");
    bugle_register_filter_set_renders("camera");
    bugle_register_filter_set_depends("camera", "trackdisplaylist");
}
