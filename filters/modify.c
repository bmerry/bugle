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
#include "common/bool.h"
#include <GL/glx.h>
#include <sys/time.h>

/* Wireframe filter-set */

static bool wireframe_callback(function_call *call, const callback_data *data)
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
        case GL_TEXTURE_CUBE_MAP_ARB:
#endif
#ifdef GL_EXT_texture3D
        case GL_TEXTURE_3D_EXT:
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
    register_filter_set_renders("wireframe");
    register_filter_post_renders("wireframe");
    return true;
}

static bool frontbuffer_callback(function_call *call, const callback_data *data)
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

typedef struct
{
    bool initialised;
    GLuint list_base;
    struct timeval last_time;
} showfps_struct;

static bool showfps_callback(function_call *call, const callback_data *data)
{
    Display *dpy;
    GLXDrawable old_read, old_write;
    GLXContext real, aux;
    Font f;

    char *ch;
    showfps_struct *s;
    struct timeval now;
    double elapsed;
    char buffer[128];

    switch (canonical_call(call))
    {
    case FUNC_glXSwapBuffers:
        aux = get_aux_context();
        if (aux && begin_internal_render())
        {
            real = glXGetCurrentContext();
            old_write = glXGetCurrentDrawable();
            old_read = glXGetCurrentReadDrawable();
            dpy = glXGetCurrentDisplay();
            glXMakeContextCurrent(dpy, old_write, old_write, aux);
            s = (showfps_struct *) data->context_data;
            if (!s->initialised)
            {
                s->initialised = true;
                s->list_base = CALL_glGenLists(256);
                f = XLoadFont(dpy, "-*-courier-*-*-*");
                glXUseXFont(f, 0, 256, s->list_base);
                XUnloadFont(dpy, f);
                gettimeofday(&s->last_time, NULL);
            }
            else
            {
                gettimeofday(&now, NULL);
                elapsed = now.tv_sec + 1.0e-6 * now.tv_usec;
                elapsed -= s->last_time.tv_sec + 1.0e-6 * s->last_time.tv_usec;
                s->last_time = now;
                snprintf(buffer, sizeof(buffer), "%.1f fps", 1.0 / elapsed);
                /* We don't want to depend on glWindowPos since it
                 * needs OpenGL 1.4, but fortunately the aux context
                 * should have identity MVP matrix.
                 */
                CALL_glPushAttrib(GL_CURRENT_BIT);
                CALL_glRasterPos2f(-0.9, -0.9);
                for (ch = buffer; *ch; ch++)
                    CALL_glCallList(s->list_base + *ch);
                CALL_glPopAttrib();
            }
            glXMakeContextCurrent(dpy, old_write, old_read, real);
            end_internal_render("showfps_callback", true);
        }
    }
    return true;
}

void initialise_filter_library(void)
{
    const filter_set_info wireframe_info =
    {
        "wireframe",
        initialise_wireframe,
        NULL,
        NULL,
        0,
        0
    };
    const filter_set_info frontbuffer_info =
    {
        "frontbuffer",
        initialise_frontbuffer,
        NULL,
        NULL,
        0,
        0
    };
    register_filter_set(&wireframe_info);
    register_filter_set(&frontbuffer_info);
}
