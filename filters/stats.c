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
#include "src/tracker.h"
#include "src/log.h"
#include "common/bool.h"
#include <stdio.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <sys/time.h>

typedef struct
{
    bool initialised;
    bool font_initialised;
    GLuint font_base;
    float fps;
    struct timeval last_time;
} stats_struct;

static filter_set *stats_handle = NULL;

static bool stats_callback(function_call *call, const callback_data *data)
{
    stats_struct *s;
    struct timeval now;
    float elapsed;
    FILE *f;

    switch (canonical_call(call))
    {
    case FUNC_glXSwapBuffers:
        s = (stats_struct *) data->context_data;
        if (!s->initialised)
        {
            s->initialised = true;
            s->fps = 0.0f;
            gettimeofday(&s->last_time, NULL);
        }
        else
        {
            gettimeofday(&now, NULL);
            elapsed = (now.tv_sec - s->last_time.tv_sec)
                + 1.0e-6 * (now.tv_usec - s->last_time.tv_usec);
            s->last_time = now;
            s->fps = 1.0f / elapsed;
        }

        if ((f = log_header("stats", "fps")) != NULL)
            fprintf(f, "%.3f\n", s->fps);
        break;
    }
    return true;
}

static bool showstats_callback(function_call *call, const callback_data *data)
{
    Display *dpy;
    GLXDrawable old_read, old_write;
    GLXContext aux, real;
    Font f;
    char buffer[128];
    char *ch;
    stats_struct *s;

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
            s = get_filter_set_context_state(tracker_get_context_state(), stats_handle);
            if (!s->font_initialised)
            {
                s->font_initialised = true;
                s->font_base = CALL_glGenLists(256);
                f = XLoadFont(dpy, "-*-courier-*-*-*");
                glXUseXFont(f, 0, 256, s->font_base);
                XUnloadFont(dpy, f);
            }
            snprintf(buffer, sizeof(buffer), "%.1f fps", s->fps);
            /* We don't want to depend on glWindowPos since it
             * needs OpenGL 1.4, but fortunately the aux context
             * should have identity MVP matrix.
             */
            CALL_glPushAttrib(GL_CURRENT_BIT);
            CALL_glRasterPos2f(-0.9, -0.9);
            for (ch = buffer; *ch; ch++)
                CALL_glCallList(s->font_base + *ch);
            CALL_glPopAttrib();
            glXMakeContextCurrent(dpy, old_write, old_read, real);
            end_internal_render("showstats_callback", true);
        }
        break;
    }
    return true;
}

static bool initialise_stats(filter_set *handle)
{
    stats_handle = handle;
    register_filter(handle, "stats", stats_callback);
    register_filter_depends("invoke", "stats");
    log_register_filter("stats");
    register_filter_set_uses_state("stats");
    return true;
}

static bool initialise_showstats(filter_set *handle)
{
    register_filter(handle, "showstats", showstats_callback);
    register_filter_depends("showstats", "stats");
    register_filter_depends("invoke", "showstats");
    register_filter_set_depends("showstats", "stats");
    register_filter_set_renders("showstats");
    return true;
}

void initialise_filter_library(void)
{
    const filter_set_info stats_info =
    {
        "stats",
        initialise_stats,
        NULL,
        NULL,
        0,
        sizeof(stats_struct)
    };
    const filter_set_info showstats_info =
    {
        "showstats",
        initialise_showstats,
        NULL,
        NULL,
        0,
        0
    };
    register_filter_set(&stats_info);
    register_filter_set(&showstats_info);
}
