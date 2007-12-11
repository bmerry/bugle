/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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
#include "src/glexts.h"
#include "src/glfuncs.h"
#include "src/log.h"
#include <stdbool.h>
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include <GL/glx.h>
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "xalloc.h"

#if !HAVE_SINF
# define sinf(x) ((float) (sin((double) (x))))
# define cosf(x) ((float) (cos((double) (x))))
#endif

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
    bool real;
} classify_context;

typedef struct
{
    void (*callback)(bool, void *);
    void *arg;
} classify_callback;

static void register_classify_callback(void (*callback)(bool, void *), void *arg)
{
    classify_callback *cb;

    cb = XMALLOC(classify_callback);
    cb->callback = callback;
    cb->arg = arg;
    bugle_list_append(&classify_callbacks, cb);
}

static void initialise_classify_context(const void *key, void *data)
{
    classify_context *ctx;

    ctx = (classify_context *) data;
    ctx->real = true;
}

#ifdef GL_EXT_framebuffer_object
static bool classify_glBindFramebufferEXT(function_call *call, const callback_data *data)
{
    classify_context *ctx;
    GLint fbo;
    linked_list_node *i;
    classify_callback *cb;

    ctx = (classify_context *) bugle_object_get_current_data(&bugle_context_class, classify_view);
    if (ctx && bugle_begin_internal_render())
    {
#ifdef GL_EXT_framebuffer_blit
        if (bugle_gl_has_extension(BUGLE_GL_EXT_framebuffer_blit))
        {
            CALL_glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT, &fbo);
        }
        else
#endif
        {
            CALL_glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &fbo);
        }
        ctx->real = (fbo == 0);
        bugle_end_internal_render("classify_glBindFramebufferEXT", true);
        for (i = bugle_list_head(&classify_callbacks); i; i = bugle_list_next(i))
        {
            cb = (classify_callback *) bugle_list_data(i);
            (*cb->callback)(ctx->real, cb->arg);
        }
    }
    return true;
}
#endif

static bool initialise_classify(filter_set *handle)
{
    filter *f;

    f = bugle_filter_register(handle, "classify");
#ifdef GL_EXT_framebuffer_object
    bugle_filter_catches(f, GROUP_glBindFramebufferEXT, true, classify_glBindFramebufferEXT);
#endif
    bugle_filter_order("invoke", "classify");
    bugle_filter_post_renders("classify");
    classify_view = bugle_object_view_register(&bugle_context_class, initialise_classify_context,
                                                NULL, sizeof(classify_context));
    bugle_list_init(&classify_callbacks, true);
    return true;
}

static void destroy_classify(filter_set *handle)
{
    bugle_list_clear(&classify_callbacks);
}

/*** Wireframe filter-set ***/

static filter_set *wireframe_filterset;
static object_view wireframe_view;

typedef struct
{
    bool active;            /* True if polygon mode has been adjusted */
    bool real;              /* True unless this is a render-to-X preprocess */
    GLint polygon_mode[2];  /* The app polygon mode, for front and back */
} wireframe_context;

static void initialise_wireframe_context(const void *key, void *data)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) data;
    ctx->active = false;
    ctx->real = true;
    ctx->polygon_mode[0] = GL_FILL;
    ctx->polygon_mode[1] = GL_FILL;

    if (bugle_filter_set_is_active(wireframe_filterset)
        && bugle_begin_internal_render())
    {
        CALL_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        ctx->real = true;
        bugle_end_internal_render("initialise_wireframe_context", true);
    }
}

/* Handles on the fly activation and deactivation */
static void wireframe_handle_activation(bool active, wireframe_context *ctx)
{
    active &= ctx->real;
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
    /* apps that render the entire frame don't always bother to clear */
    CALL_glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

static bool wireframe_glPolygonMode(function_call *call, const callback_data *data)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(&bugle_context_class, wireframe_view);
    if (ctx && !ctx->real)
        return true;
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
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(&bugle_context_class, wireframe_view);
    if (ctx && !ctx->real)
        return true;
    /* FIXME: need to track this state to restore it on deactivation */
    /* FIXME: also need to nuke it at activation */
    /* FIXME: has no effect when using a fragment shader */
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

static void wireframe_classify_callback(bool real, void *arg)
{
    wireframe_context *ctx;

    ctx = (wireframe_context *) bugle_object_get_current_data(&bugle_context_class, wireframe_view);
    if (ctx)
    {
        ctx->real = real;
        wireframe_handle_activation(bugle_filter_set_is_active((filter_set *) arg), ctx);
    }
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
    f = bugle_filter_register(handle, "wireframe");
    bugle_filter_catches(f, GROUP_glXSwapBuffers, false, wireframe_glXSwapBuffers);
    bugle_filter_catches(f, GROUP_glPolygonMode, false, wireframe_glPolygonMode);
    bugle_filter_catches(f, GROUP_glEnable, false, wireframe_glEnable);
    bugle_filter_catches(f, GROUP_glXMakeCurrent, true, wireframe_make_current);
#ifdef GLX_SGI_make_current_read
    bugle_filter_catches(f, GROUP_glXMakeCurrentReadSGI, true, wireframe_make_current);
#endif
    bugle_filter_order("invoke", "wireframe");
    bugle_filter_post_renders("wireframe");
    wireframe_view = bugle_object_view_register(&bugle_context_class, initialise_wireframe_context,
                                                 NULL, sizeof(wireframe_context));
    register_classify_callback(wireframe_classify_callback, handle);
    return true;
}

/*** Frontbuffer filterset */

static filter_set *frontbuffer_filterset;
static object_view frontbuffer_view;

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
    f = bugle_filter_register(handle, "frontbuffer");
    bugle_filter_order("invoke", "frontbuffer");
    bugle_filter_catches(f, GROUP_glDrawBuffer, false, frontbuffer_glDrawBuffer);
    bugle_filter_catches(f, GROUP_glXMakeCurrent, true, frontbuffer_make_current);
#ifdef GLX_SGI_make_current_read
    bugle_filter_catches(f, GROUP_glXMakeCurrentReadSGI, true, frontbuffer_make_current);
#endif
    bugle_filter_post_renders("frontbuffer");

    f = bugle_filter_register(handle, "frontbuffer_pre");
    bugle_filter_order("frontbuffer_pre", "invoke");
    bugle_filter_catches(f, GROUP_glXSwapBuffers, false, frontbuffer_glXSwapBuffers);

    frontbuffer_view = bugle_object_view_register(&bugle_context_class, initialise_frontbuffer_context,
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
#define CAMERA_KEY_FRUSTUM 8
#define CAMERA_KEYS 9

static filter_set *camera_filterset;
static object_view camera_view;
static float camera_speed = 1.0f;
static bool camera_dga = false;
static bool camera_intercept = true;
static bool camera_frustum = false;

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

/* Determinant of a 4x4 matrix */
static GLfloat determinant(const GLfloat *m)
{
    return m[0] * (  m[5]  * (m[10] * m[15] - m[11] * m[14])
                   + m[6]  * (m[11] * m[13] - m[9]  * m[15])
                   + m[7]  * (m[9]  * m[14] - m[10] * m[13]))
        -  m[1] * (  m[6]  * (m[11] * m[12] - m[8]  * m[15])
                   + m[7]  * (m[8]  * m[14] - m[10] * m[12])
                   + m[4]  * (m[10] * m[15] - m[11] * m[14]))
        +  m[2] * (  m[7]  * (m[8]  * m[13] - m[9]  * m[12])
                   + m[4]  * (m[9]  * m[15] - m[11] * m[13])
                   + m[5]  * (m[11] * m[12] - m[8]  * m[15]))
        -  m[3] * (  m[4]  * (m[9]  * m[14] - m[10] * m[13])
                   + m[5]  * (m[10] * m[12] - m[8]  * m[14])
                   + m[6]  * (m[8]  * m[13] - m[9]  * m[12]));
}

/* Solves mx = b|m| i.e. divide by determinant to get denominator */
static void solve_numerator(const GLfloat *m, const GLfloat *b, GLfloat *x)
{
    GLfloat tmp[16];
    int i;

    /* Cramer's rule: replace each row in turn with b and take the
     * determinants.
     */
    memcpy(tmp, m, 16 * sizeof(GLfloat));
    for (i = 0; i < 4; i++)
    {
        memcpy(tmp + i * 4, b, 4 * sizeof(GLfloat));
        x[i] = determinant(tmp);
        memcpy(tmp + i * 4, m + i * 4, 4 * sizeof(GLfloat));
    }
}

/* Generates 8 4-vectors corresponding to the 8 vertices */
static void frustum_vertices(const GLfloat *m, GLfloat *x)
{
    GLfloat b[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    GLfloat w;
    int i, j;
    for (i = 0; i < 8; i++)
    {
        b[0] = (i & 1) ? -1.0f : 1.0f;
        b[1] = (i & 2) ? -1.0f : 1.0f;
        b[2] = (i & 4) ? -1.0f : 1.0f;
        solve_numerator(m, b, x + 4 * i);
        w = x[4 * i + 3];
        if (w)
        {
            for (j = 0; j < 4; j++)
                x[4 * i + j] /= w;
        }
    }
}

static void camera_draw_frustum(const GLfloat *original)
{
    Display *dpy;
    GLXDrawable old_read, old_write;
    GLXContext real, aux;
    int i, j, k;

    GLubyte indices[24] =
    {
        0, 2, 3, 1,
        4, 5, 7, 6,
        0, 1, 5, 4,
        2, 6, 7, 3,
        0, 4, 6, 2,
        1, 3, 7, 5
    };
    GLfloat vertices[32];
    GLfloat modelview[16];
    GLfloat projection[16];
    GLfloat mvp[16];
    GLint viewport[4];

    aux = bugle_get_aux_context(true);
    if (!aux) return;

    CALL_glGetIntegerv(GL_VIEWPORT, viewport);
    CALL_glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    CALL_glGetFloatv(GL_PROJECTION_MATRIX, projection);
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
        {
            mvp[i * 4 + j] = 0.0f;
            for (k = 0; k < 4; k++)
                mvp[i * 4 + j] += projection[k * 4 + j] * original[i * 4 + k];
        }
    frustum_vertices(mvp, vertices);

    real = CALL_glXGetCurrentContext();
    old_write = CALL_glXGetCurrentDrawable();
    old_read = bugle_get_current_read_drawable();
    dpy = bugle_get_current_display();
    CALL_glXMakeCurrent(dpy, old_write, aux);

    CALL_glPushAttrib(GL_VIEWPORT_BIT);
    CALL_glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    CALL_glMatrixMode(GL_PROJECTION);
    CALL_glLoadMatrixf(projection);
    CALL_glMatrixMode(GL_MODELVIEW);
    CALL_glLoadMatrixf(modelview);
    CALL_glEnable(GL_BLEND);
    CALL_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    CALL_glEnable(GL_CULL_FACE);
    CALL_glEnable(GL_DEPTH_TEST);
    CALL_glDepthMask(GL_FALSE);
    CALL_glVertexPointer(4, GL_FLOAT, 0, vertices);
    CALL_glEnableClientState(GL_VERTEX_ARRAY);
#ifdef GL_NV_depth_clamp
    if (bugle_gl_has_extension(BUGLE_GL_NV_depth_clamp))
    {
        glEnable(GL_DEPTH_CLAMP_NV);
        glDepthFunc(GL_LEQUAL);
    }
#endif

    for (i = 0; i < 2; i++)
    {
        CALL_glFrontFace(i ? GL_CCW : GL_CW);
        CALL_glColor4f(0.2f, 0.2f, 1.0f, 0.5f);
        CALL_glDrawElements(GL_QUADS, 24, GL_UNSIGNED_BYTE, indices);
        CALL_glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        CALL_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        CALL_glDrawElements(GL_QUADS, 24, GL_UNSIGNED_BYTE, indices);
        CALL_glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

#ifdef GL_NV_depth_clamp
    if (bugle_gl_has_extension(BUGLE_GL_NV_depth_clamp))
    {
        glDepthFunc(GL_LESS);
        glDisable(GL_DEPTH_CLAMP_NV);
    }
#endif
    CALL_glMatrixMode(GL_PROJECTION);
    CALL_glLoadIdentity();
    CALL_glMatrixMode(GL_MODELVIEW);
    CALL_glLoadIdentity();
    CALL_glDisable(GL_DEPTH_TEST);
    CALL_glBlendFunc(GL_ONE, GL_ZERO);
    CALL_glDisable(GL_BLEND);
    CALL_glDisable(GL_CULL_FACE);
    CALL_glDepthMask(GL_TRUE);
    CALL_glVertexPointer(4, GL_FLOAT, 0, NULL);
    CALL_glDisableClientState(GL_VERTEX_ARRAY);
    CALL_glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    CALL_glPopAttrib();
    bugle_make_context_current(dpy, old_write, old_read, real);
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
        case CAMERA_KEY_FRUSTUM:
            camera_frustum = !camera_frustum;
            invalidate_window(event);
            break;
        }
    }
}

static void camera_get_original(camera_context *ctx)
{
    CALL_glGetFloatv(GL_MODELVIEW_MATRIX, ctx->original);
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
        camera_get_original(ctx);
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
                camera_get_original(ctx);
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

        if (camera_frustum)
            camera_draw_frustum(ctx->original);
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
            camera_get_original(ctx);
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
    f = bugle_filter_register(handle, "camera_pre");
    bugle_filter_order("camera_pre", "invoke");
    bugle_filter_catches(f, GROUP_glMultMatrixf, false, camera_restore);
    bugle_filter_catches(f, GROUP_glMultMatrixd, false, camera_restore);
    bugle_filter_catches(f, GROUP_glTranslatef, false, camera_restore);
    bugle_filter_catches(f, GROUP_glTranslated, false, camera_restore);
    bugle_filter_catches(f, GROUP_glScalef, false, camera_restore);
    bugle_filter_catches(f, GROUP_glScaled, false, camera_restore);
    bugle_filter_catches(f, GROUP_glRotatef, false, camera_restore);
    bugle_filter_catches(f, GROUP_glRotated, false, camera_restore);
    bugle_filter_catches(f, GROUP_glFrustum, false, camera_restore);
    bugle_filter_catches(f, GROUP_glPushMatrix, false, camera_restore);
    bugle_filter_catches(f, GROUP_glPopMatrix, false, camera_restore);
#ifdef GL_ARB_transpose_matrix
    bugle_filter_catches(f, GROUP_glMultTransposeMatrixf, false, camera_restore);
    bugle_filter_catches(f, GROUP_glMultTransposeMatrixd, false, camera_restore);
#endif
    bugle_filter_catches(f, GROUP_glXSwapBuffers, false, camera_glXSwapBuffers);

    f = bugle_filter_register(handle, "camera_post");
    bugle_filter_post_renders("camera_post");
    bugle_filter_order("invoke", "camera_post");
    bugle_filter_catches(f, GROUP_glXMakeCurrent, true, camera_make_current);
#ifdef GLX_SGI_make_current_read
    bugle_filter_catches(f, GROUP_glXMakeCurrentReadSGI, true, camera_make_current);
#endif
    bugle_filter_catches(f, GROUP_glLoadIdentity, false, camera_override);
    bugle_filter_catches(f, GROUP_glLoadMatrixf, false, camera_override);
    bugle_filter_catches(f, GROUP_glLoadMatrixd, false, camera_override);
    bugle_filter_catches(f, GROUP_glMultMatrixf, false, camera_override);
    bugle_filter_catches(f, GROUP_glMultMatrixd, false, camera_override);
    bugle_filter_catches(f, GROUP_glTranslatef, false, camera_override);
    bugle_filter_catches(f, GROUP_glTranslated, false, camera_override);
    bugle_filter_catches(f, GROUP_glScalef, false, camera_override);
    bugle_filter_catches(f, GROUP_glScaled, false, camera_override);
    bugle_filter_catches(f, GROUP_glRotatef, false, camera_override);
    bugle_filter_catches(f, GROUP_glRotated, false, camera_override);
    bugle_filter_catches(f, GROUP_glFrustum, false, camera_override);
    bugle_filter_catches(f, GROUP_glPushMatrix, false, camera_override);
    bugle_filter_catches(f, GROUP_glPopMatrix, false, camera_override);
#ifdef GL_ARB_transpose_matrix
    bugle_filter_catches(f, GROUP_glLoadTransposeMatrixf, false, camera_override);
    bugle_filter_catches(f, GROUP_glLoadTransposeMatrixd, false, camera_override);
    bugle_filter_catches(f, GROUP_glMultTransposeMatrixf, false, camera_override);
    bugle_filter_catches(f, GROUP_glMultTransposeMatrixd, false, camera_override);
#endif
    bugle_filter_catches(f, GROUP_glGetFloatv, false, camera_get);
    bugle_filter_catches(f, GROUP_glGetDoublev, false, camera_get);

    camera_view = bugle_object_view_register(&bugle_context_class, initialise_camera_context,
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

/*** extoverride extension ***/

/* The design is that extensions that we have never heard of may be
 * suppressed; hence we use the two hash-tables rather than a boolean array.
 */
static bool extoverride_disable_all = false;
static char *extoverride_max_version = NULL;
static hash_table extoverride_disabled;
static hash_table extoverride_enabled;
static object_view extoverride_view;

typedef struct
{
    const GLubyte *version; /* Filtered version of glGetString(GL_VERSION) */
    GLubyte *extensions;    /* Filtered version of glGetString(GL_EXTENSIONS) */
} extoverride_context;

static bool extoverride_suppressed(const char *ext)
{
    if (extoverride_disable_all)
        return !bugle_hash_count(&extoverride_enabled, ext);
    else
        return bugle_hash_count(&extoverride_disabled, ext);
}

static void initialise_extoverride_context(const void *key, void *data)
{
    const char *real_version, *real_exts, *p, *q;
    char *exts, *ext, *exts_ptr;
    extoverride_context *self;

    real_version = (const char *) CALL_glGetString(GL_VERSION);
    real_exts = (const char *) CALL_glGetString(GL_EXTENSIONS);
    self = (extoverride_context *) data;

    if (extoverride_max_version && strcmp(real_version, extoverride_max_version) > 0)
        self->version = (const GLubyte *) extoverride_max_version;
    else
        self->version = (const GLubyte *) real_version;

    exts_ptr = exts = XNMALLOC(strlen(real_exts) + 1, char);
    ext = XNMALLOC(strlen(real_exts) + 1, char); /* Current extension */
    p = real_exts;
    while (*p == ' ') p++;
    while (*p)
    {
        q = p;
        while (*q != ' ' && *q != '\0') q++;
        memcpy(ext, p, q - p);
        ext[q - p] = '\0';
        if (!extoverride_suppressed(ext))
        {
            memcpy(exts_ptr, ext, q - p);
            exts_ptr += q - p;
            *exts_ptr++ = ' ';
        }
        p = q;
        while (*p == ' ') p++;
    }

    /* Back up over the trailing space */
    if (exts_ptr != exts) exts_ptr--;
    *exts_ptr = '\0';
    self->extensions = (GLubyte *) exts;
    free(ext);
}

static void destroy_extoverride_context(void *data)
{
    extoverride_context *self;

    self = (extoverride_context *) data;
    /* self->version is always a shallow copy of another string */
    free(self->extensions);
}

static bool extoverride_glGetString(function_call *call, const callback_data *data)
{
    extoverride_context *ctx;

    ctx = (extoverride_context *) bugle_object_get_current_data(&bugle_context_class, extoverride_view);
    if (!ctx) return true; /* Nothing can be overridden */
    switch (*call->typed.glGetString.arg0)
    {
    case GL_VERSION:
        *call->typed.glGetString.retn = ctx->version;
        break;
    case GL_EXTENSIONS:
        *call->typed.glGetString.retn = ctx->extensions;
        break;
    }
    return true;
}

static bool extoverride_warn(function_call *call, const callback_data *data)
{
    bugle_log_printf("extoverride", "warn", BUGLE_LOG_NOTICE,
                     "%s was called, although the corresponding extension was suppressed",
                     budgie_function_table[call->generic.id].name);
    return true;
}

static bool initialise_extoverride(filter_set *handle)
{
    filter *f;
    budgie_function i;

    f = bugle_filter_register(handle, "extoverride_get");
    bugle_filter_order("invoke", "extoverride_get");
    bugle_filter_order("extoverride_get", "trace");
    bugle_filter_catches(f, GROUP_glGetString, false, extoverride_glGetString);

    f = bugle_filter_register(handle, "extoverride_warn");
    bugle_filter_order("extoverride_warn", "invoke");
    for (i = 0; i < budgie_number_of_functions; i++)
    {
        if (bugle_gl_function_table[i].extension
            && extoverride_suppressed(bugle_gl_function_table[i].extension))
            bugle_filter_catches_function(f, i, false, extoverride_warn);
        else if (extoverride_max_version
                 && bugle_gl_function_table[i].version
                 && bugle_gl_function_table[i].version[2] == '_' /* filter out GLX */
                 && strcmp(bugle_gl_function_table[i].version, extoverride_max_version) > 1)
            bugle_filter_catches_function(f, i, false, extoverride_warn);
    }

    extoverride_view = bugle_object_view_register(&bugle_context_class, initialise_extoverride_context,
                                                   destroy_extoverride_context, sizeof(extoverride_context));
    return true;
}

static void destroy_extoverride(filter_set *handle)
{
    bugle_hash_clear(&extoverride_disabled);
    bugle_hash_clear(&extoverride_enabled);
    free(extoverride_max_version);
}

static bool extoverride_variable_disable(const struct filter_set_variable_info_s *var,
                                         const char *text, const void *value)
{
    bool found = false;
    size_t i;

    if (strcmp(text, "all") == 0)
    {
        extoverride_disable_all = true;
        return true;
    }

    bugle_hash_set(&extoverride_disabled, text, NULL);
    for (i = 0; i < BUGLE_EXT_COUNT; i++)
        if (!bugle_exts[i].glx && bugle_exts[i].glext_string
            && strcmp(bugle_exts[i].glext_string, text) == 0)
        {
            found = true;
            break;
        }
    if (!found)
        bugle_log_printf("extoverride", "disable", BUGLE_LOG_WARNING,
                         "Extension %s is unknown (typo?)", text);
    return true;
}

static bool extoverride_variable_enable(const struct filter_set_variable_info_s *var,
                                        const char *text, const void *value)
{
    bool found = false;
    size_t i;

    if (strcmp(text, "all") == 0)
    {
        extoverride_disable_all = false;
        return true;
    }

    bugle_hash_set(&extoverride_enabled, text, NULL);
    for (i = 0; i < BUGLE_EXT_COUNT; i++)
        if (!bugle_exts[i].glx && bugle_exts[i].glext_string
            && strcmp(bugle_exts[i].glext_string, text) == 0)
        {
            found = true;
            break;
        }
    if (!found)
        bugle_log_printf("extoverride", "enable", BUGLE_LOG_WARNING,
                         "Extension %s is unknown (typo?)", text);
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info classify_info =
    {
        "classify",
        initialise_classify,
        destroy_classify,
        NULL,
        NULL,
        NULL,
        NULL
    };
    static const filter_set_info wireframe_info =
    {
        "wireframe",
        initialise_wireframe,
        NULL,
        wireframe_activation,
        wireframe_deactivation,
        NULL,
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
        { "key_frustum", "key to enable drawing the frustum [none]", FILTER_SET_VARIABLE_KEY, &key_camera[CAMERA_KEY_FRUSTUM], NULL },
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
        "allows the camera position to be changed on the fly"
    };

    static const filter_set_variable_info extoverride_variables[] =
    {
        { "disable", "name of an extension to suppress, or 'all'", FILTER_SET_VARIABLE_CUSTOM, NULL, extoverride_variable_disable },
        { "enable", "name of an extension to enable, or 'all'", FILTER_SET_VARIABLE_CUSTOM, NULL, extoverride_variable_enable },
        { "version", "maximum OpenGL version to expose", FILTER_SET_VARIABLE_STRING, &extoverride_max_version, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info extoverride_info =
    {
        "extoverride",
        initialise_extoverride,
        destroy_extoverride,
        NULL,
        NULL,
        extoverride_variables,
        "suppresses extensions or OpenGL versions"
    };

    bugle_filter_set_register(&classify_info);
    bugle_filter_set_register(&wireframe_info);
    bugle_filter_set_register(&frontbuffer_info);
    bugle_filter_set_register(&camera_info);
    bugle_filter_set_register(&extoverride_info);

    bugle_filter_set_renders("classify");
    bugle_filter_set_depends("classify", "trackcontext");
    bugle_filter_set_depends("classify", "trackextensions");
    bugle_filter_set_renders("wireframe");
    bugle_filter_set_depends("wireframe", "trackcontext");
    bugle_filter_set_depends("wireframe", "classify");
    bugle_filter_set_renders("frontbuffer");
    bugle_filter_set_renders("camera");
    bugle_filter_set_depends("camera", "classify");
    bugle_filter_set_depends("camera", "trackdisplaylist");
    bugle_filter_set_depends("camera", "trackcontext");
    bugle_filter_set_depends("camera", "trackextensions");
    bugle_filter_set_depends("extoverride", "trackextensions");
}
