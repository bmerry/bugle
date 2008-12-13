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
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glextensions.h>
#include <bugle/gl/gldisplaylist.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/input.h>
#include <bugle/apireflect.h>
#include <budgie/reflect.h>
#include <budgie/addresses.h>

#if !HAVE_SINF
# define sinf(x) ((float) (sin((double) (x))))
# define cosf(x) ((float) (cos((double) (x))))
#endif

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

static bugle_input_key key_camera[CAMERA_KEYS];
static const char * const key_camera_defaults[] =
{
    "Up",
    "Down",
    "Left",
    "Right",
    "Page_Up",
    "Page_Down"
};

typedef struct
{
    GLfloat original[16];
    GLfloat modifier[16];
    bool active;
    bool dirty;       /* Set to true when mouse moves */
    bool pressed[CAMERA_MOTION_KEYS];
} camera_context;

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
    glwin_display dpy;
    glwin_drawable old_read, old_write;
    glwin_context real, aux;
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

    CALL(glGetIntegerv)(GL_VIEWPORT, viewport);
    CALL(glGetFloatv)(GL_MODELVIEW_MATRIX, modelview);
    CALL(glGetFloatv)(GL_PROJECTION_MATRIX, projection);
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
        {
            mvp[i * 4 + j] = 0.0f;
            for (k = 0; k < 4; k++)
                mvp[i * 4 + j] += projection[k * 4 + j] * original[i * 4 + k];
        }
    frustum_vertices(mvp, vertices);

    real = bugle_glwin_get_current_context();
    old_write = bugle_glwin_get_current_drawable();
    old_read = bugle_glwin_get_current_read_drawable();
    dpy = bugle_glwin_get_current_display();
    bugle_glwin_make_context_current(dpy, old_write, old_write, aux);

    CALL(glPushAttrib)(GL_VIEWPORT_BIT);
    CALL(glViewport)(viewport[0], viewport[1], viewport[2], viewport[3]);
    CALL(glMatrixMode)(GL_PROJECTION);
    CALL(glLoadMatrixf)(projection);
    CALL(glMatrixMode)(GL_MODELVIEW);
    CALL(glLoadMatrixf)(modelview);
    CALL(glEnable)(GL_BLEND);
    CALL(glBlendFunc)(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    CALL(glEnable)(GL_CULL_FACE);
    CALL(glEnable)(GL_DEPTH_TEST);
    CALL(glDepthMask)(GL_FALSE);
    CALL(glVertexPointer)(4, GL_FLOAT, 0, vertices);
    CALL(glEnableClientState)(GL_VERTEX_ARRAY);
#ifdef GL_NV_depth_clamp
    if (BUGLE_GL_HAS_EXTENSION(GL_NV_depth_clamp))
    {
        CALL(glEnable)(GL_DEPTH_CLAMP_NV);
        CALL(glDepthFunc)(GL_LEQUAL);
    }
#endif

    for (i = 0; i < 2; i++)
    {
        CALL(glFrontFace)(i ? GL_CCW : GL_CW);
        CALL(glColor4f)(0.2f, 0.2f, 1.0f, 0.5f);
        CALL(glDrawElements)(GL_QUADS, 24, GL_UNSIGNED_BYTE, indices);
        CALL(glColor4f)(1.0f, 1.0f, 1.0f, 1.0f);
        CALL(glPolygonMode)(GL_FRONT_AND_BACK, GL_LINE);
        CALL(glDrawElements)(GL_QUADS, 24, GL_UNSIGNED_BYTE, indices);
        CALL(glPolygonMode)(GL_FRONT_AND_BACK, GL_FILL);
    }

#ifdef GL_NV_depth_clamp
    if (BUGLE_GL_HAS_EXTENSION(GL_NV_depth_clamp))
    {
        CALL(glDepthFunc)(GL_LESS);
        CALL(glDisable)(GL_DEPTH_CLAMP_NV);
    }
#endif
    CALL(glMatrixMode)(GL_PROJECTION);
    CALL(glLoadIdentity)();
    CALL(glMatrixMode)(GL_MODELVIEW);
    CALL(glLoadIdentity)();
    CALL(glDisable)(GL_DEPTH_TEST);
    CALL(glBlendFunc)(GL_ONE, GL_ZERO);
    CALL(glDisable)(GL_BLEND);
    CALL(glDisable)(GL_CULL_FACE);
    CALL(glDepthMask)(GL_TRUE);
    CALL(glVertexPointer)(4, GL_FLOAT, 0, NULL);
    CALL(glDisableClientState)(GL_VERTEX_ARRAY);
    CALL(glColor4f)(1.0f, 1.0f, 1.0f, 1.0f);
    CALL(glPopAttrib)();
    bugle_glwin_make_context_current(dpy, old_write, old_read, real);
}

static void camera_mouse_callback(int dx, int dy, bugle_input_event *event)
{
    camera_context *ctx;
    GLfloat old[16], rotator[3][3];
    GLfloat axis[3];
    GLfloat angle, cs, sn;
    int i, j, k;

    ctx = (camera_context *) bugle_object_get_current_data(bugle_context_class, camera_view);
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

    bugle_input_invalidate_window(event);
}

static bool camera_key_wanted(const bugle_input_key *key, void *arg, bugle_input_event *event)
{
    return bugle_filter_set_is_active(camera_filterset);
}

static void camera_key_callback(const bugle_input_key *key, void *arg, bugle_input_event *event)
{
    int index, i;
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(bugle_context_class, camera_view);
    if (!ctx) return;
    index = (bugle_input_key *) arg - key_camera;
    if (index < CAMERA_MOTION_KEYS)
    {
        ctx->pressed[index] = key->press;
        if (key->press) bugle_input_invalidate_window(event);
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
            bugle_input_invalidate_window(event);
            break;
        case CAMERA_KEY_TOGGLE:
            if (!camera_intercept)
            {
                camera_intercept = true;
                bugle_input_grab_pointer(camera_dga, camera_mouse_callback);
                break;
            }
            else
            {
                camera_intercept = false;
                bugle_input_release_pointer();
            }
            break;
        case CAMERA_KEY_FRUSTUM:
            camera_frustum = !camera_frustum;
            bugle_input_invalidate_window(event);
            break;
        }
    }
}

static void camera_get_original(camera_context *ctx)
{
    CALL(glGetFloatv)(GL_MODELVIEW_MATRIX, ctx->original);
}

static void camera_context_init(const void *key, void *data)
{
    camera_context *ctx;
    int i;

    ctx = (camera_context *) data;
    for (i = 0; i < 4; i++)
        ctx->modifier[i * 5] = 1.0f;

    if (bugle_filter_set_is_active(camera_filterset)
        && bugle_gl_begin_internal_render())
    {
        ctx->active = true;
        camera_get_original(ctx);
        bugle_gl_end_internal_render("camera_context_init", true);
    }
}

static bool camera_restore(function_call *call, const callback_data *data)
{
    GLint mode;
    camera_context *ctx;

    if (bugle_displaylist_mode() == GL_NONE)
    {
        ctx = (camera_context *) bugle_object_get_current_data(bugle_context_class, camera_view);
        if (ctx && bugle_gl_begin_internal_render())
        {
            CALL(glGetIntegerv)(GL_MATRIX_MODE, &mode);
            if (mode == GL_MODELVIEW)
                CALL(glLoadMatrixf)(ctx->original);
            bugle_gl_end_internal_render("camera_restore", true);
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
        ctx = (camera_context *) bugle_object_get_current_data(bugle_context_class, camera_view);
        if (ctx && bugle_gl_begin_internal_render())
        {
            CALL(glGetIntegerv)(GL_MATRIX_MODE, &mode);
            if (mode == GL_MODELVIEW)
            {
                camera_get_original(ctx);
                CALL(glLoadMatrixf)(ctx->modifier);
                CALL(glMultMatrixf)(ctx->original);
            }
            bugle_gl_end_internal_render("camera_restore", true);
        }
    }
    return true;
}

static bool camera_get(function_call *call, const callback_data *data)
{
    int i;
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(bugle_context_class, camera_view);
    if (!ctx) return true;

    if (call->generic.group == BUDGIE_GROUP_ID(glGetFloatv))
    {
        for (i = 0; i < 16; i++)
            (*call->glGetFloatv.arg1)[i] = ctx->original[i];
    }
    else if (call->generic.group == BUDGIE_GROUP_ID(glGetDoublev))
    {
        for (i = 0; i < 16; i++)
            (*call->glGetDoublev.arg1)[i] = ctx->original[i];
    }
    return true;
}

static bool camera_swap_buffers(function_call *call, const callback_data *data)
{
    camera_context *ctx;
    GLint mode;

    ctx = (camera_context *) bugle_object_get_current_data(bugle_context_class, camera_view);
    if (ctx && bugle_gl_begin_internal_render())
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
            CALL(glGetIntegerv)(GL_MATRIX_MODE, &mode);
            CALL(glMatrixMode)(GL_MODELVIEW);
            CALL(glLoadMatrixf)(ctx->modifier);
            CALL(glMultMatrixf)(ctx->original);
            CALL(glMatrixMode)(mode);
            ctx->dirty = false;
        }
        bugle_gl_end_internal_render("camera_swap_buffers", true);
    }
    return true;
}

static void camera_handle_activation(bool active, camera_context *ctx)
{
    GLint mode;

    if (active && !ctx->active)
    {
        if (bugle_gl_begin_internal_render())
        {
            camera_get_original(ctx);
            ctx->active = true;
            ctx->dirty = true;
            bugle_gl_end_internal_render("camera_handle_activation", true);
        }
    }
    else if (!active && ctx->active)
    {
        if (bugle_gl_begin_internal_render())
        {
            CALL(glGetIntegerv)(GL_MATRIX_MODE, &mode);
            CALL(glMatrixMode)(GL_MODELVIEW);
            CALL(glLoadMatrixf)(ctx->original);
            CALL(glMatrixMode)(mode);
            ctx->active = false;
            bugle_gl_end_internal_render("camera_handle_activation", true);
        }
    }
}

static bool camera_make_current(function_call *call, const callback_data *data)
{
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(bugle_context_class, camera_view);
    if (ctx)
        camera_handle_activation(bugle_filter_set_is_active(data->filter_set_handle), ctx);
    return true;
}

static void camera_activation(filter_set *handle)
{
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(bugle_context_class, camera_view);
    if (ctx)
        camera_handle_activation(true, ctx);
    if (camera_intercept)
        bugle_input_grab_pointer(camera_dga, camera_mouse_callback);
}

static void camera_deactivation(filter_set *handle)
{
    camera_context *ctx;

    ctx = (camera_context *) bugle_object_get_current_data(bugle_context_class, camera_view);
    if (ctx)
        camera_handle_activation(false, ctx);
    if (camera_intercept)
        bugle_input_release_pointer();
}

static bool camera_initialise(filter_set *handle)
{
    filter *f;
    int i;

    camera_filterset = handle;
    f = bugle_filter_new(handle, "camera_pre");
    bugle_filter_order("camera_pre", "invoke");
    bugle_filter_catches(f, "glMultMatrixf", false, camera_restore);
    bugle_filter_catches(f, "glMultMatrixd", false, camera_restore);
    bugle_filter_catches(f, "glTranslatef", false, camera_restore);
    bugle_filter_catches(f, "glTranslated", false, camera_restore);
    bugle_filter_catches(f, "glScalef", false, camera_restore);
    bugle_filter_catches(f, "glScaled", false, camera_restore);
    bugle_filter_catches(f, "glRotatef", false, camera_restore);
    bugle_filter_catches(f, "glRotated", false, camera_restore);
    bugle_filter_catches(f, "glFrustum", false, camera_restore);
    bugle_filter_catches(f, "glPushMatrix", false, camera_restore);
    bugle_filter_catches(f, "glPopMatrix", false, camera_restore);
    bugle_filter_catches(f, "glMultTransposeMatrixf", false, camera_restore);
    bugle_filter_catches(f, "glMultTransposeMatrixd", false, camera_restore);
    bugle_glwin_filter_catches_swap_buffers(f, false, camera_swap_buffers);

    f = bugle_filter_new(handle, "camera_post");
    bugle_gl_filter_post_renders("camera_post");
    bugle_filter_order("invoke", "camera_post");
    bugle_glwin_filter_catches_make_current(f, true, camera_make_current);
    bugle_filter_catches(f, "glLoadIdentity", false, camera_override);
    bugle_filter_catches(f, "glLoadMatrixf", false, camera_override);
    bugle_filter_catches(f, "glLoadMatrixd", false, camera_override);
    bugle_filter_catches(f, "glMultMatrixf", false, camera_override);
    bugle_filter_catches(f, "glMultMatrixd", false, camera_override);
    bugle_filter_catches(f, "glTranslatef", false, camera_override);
    bugle_filter_catches(f, "glTranslated", false, camera_override);
    bugle_filter_catches(f, "glScalef", false, camera_override);
    bugle_filter_catches(f, "glScaled", false, camera_override);
    bugle_filter_catches(f, "glRotatef", false, camera_override);
    bugle_filter_catches(f, "glRotated", false, camera_override);
    bugle_filter_catches(f, "glFrustum", false, camera_override);
    bugle_filter_catches(f, "glPushMatrix", false, camera_override);
    bugle_filter_catches(f, "glPopMatrix", false, camera_override);
    bugle_filter_catches(f, "glLoadTransposeMatrixf", false, camera_override);
    bugle_filter_catches(f, "glLoadTransposeMatrixd", false, camera_override);
    bugle_filter_catches(f, "glMultTransposeMatrixf", false, camera_override);
    bugle_filter_catches(f, "glMultTransposeMatrixd", false, camera_override);
    bugle_filter_catches(f, "glGetFloatv", false, camera_get);
    bugle_filter_catches(f, "glGetDoublev", false, camera_get);

    camera_view = bugle_object_view_new(bugle_context_class,
                                        camera_context_init,
                                        NULL, sizeof(camera_context));

    for (i = 0; i < CAMERA_KEYS; i++)
    {
        bugle_input_key release;

        release = key_camera[i];
        release.press = false;
        bugle_input_key_callback(&key_camera[i], camera_key_wanted, camera_key_callback, (void *) &key_camera[i]);
        if (i < CAMERA_MOTION_KEYS)
            bugle_input_key_callback(&release, camera_key_wanted, camera_key_callback, (void *) &key_camera[i]);
    }
    return true;
}

void bugle_initialise_filter_library(void)
{
    int i;

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
        camera_initialise,
        NULL,
        camera_activation,
        camera_deactivation,
        camera_variables,
        "allows the camera position to be changed on the fly"
    };

    bugle_filter_set_new(&camera_info);

    bugle_gl_filter_set_renders("camera");
    bugle_filter_set_depends("camera", "classify");
    bugle_filter_set_depends("camera", "gldisplaylist");
    bugle_filter_set_depends("camera", "trackcontext");
    bugle_filter_set_depends("camera", "glextensions");

    for (i = 0; i < CAMERA_KEYS; i++)
    {
        key_camera[i].keysym = BUGLE_INPUT_NOSYMBOL;
        key_camera[i].state = 0;
        if (key_camera_defaults[i] != NULL)
            bugle_input_key_lookup(key_camera_defaults[i], &key_camera[i]);
    }
}
