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
    /* resources */
    bool initialised;
    bool font_initialised;
    GLuint font_base;
    GLuint query;
    struct timeval last_time;

    /* stats */
    float fps;
    GLuint fragments;
} stats_struct;

static filter_set *stats_handle = NULL;
static bool count_fragments = false;

static void init_stats_struct(stats_struct *s)
{
    s->initialised = true;
    s->query = 0;
    gettimeofday(&s->last_time, NULL);
#ifdef GL_ARB_occlusion_query
    if (count_fragments && function_table[FUNC_glGenQueriesARB].real)
        CALL_glGenQueriesARB(1, &s->query);
#endif

    s->fps = 0.0f;
    s->fragments = 0;
}

static bool stats_callback(function_call *call, const callback_data *data)
{
    stats_struct *s;
    struct timeval now;
    float elapsed;
    FILE *f;

    switch (canonical_call(call))
    {
    case CFUNC_glXSwapBuffers:
        s = (stats_struct *) data->context_data;
        if (!s->initialised) init_stats_struct(s);
        else
        {
            gettimeofday(&now, NULL);
            elapsed = (now.tv_sec - s->last_time.tv_sec)
                + 1.0e-6 * (now.tv_usec - s->last_time.tv_usec);
            s->last_time = now;
            s->fps = 1.0f / elapsed;
#ifdef GL_ARB_occlusion_query
            if (s->query && begin_internal_render())
            {
                CALL_glEndQueryARB(GL_SAMPLES_PASSED);
                CALL_glGetQueryObjectuivARB(s->query, GL_QUERY_RESULT_ARB, &s->fragments);
                end_internal_render("stats_callback", true);
            }
            else
#endif
            {
                s->fragments = 0;
            }
        }

        if ((f = log_header("stats", "fps")) != NULL)
            fprintf(f, "%.3f\n", s->fps);
        if (s->query && (f = log_header("stats", "fragments")) != NULL)
            fprintf(f, "%u\n", (unsigned int) s->fragments);
        break;
#ifdef CFUNC_glBeginQueryARB
    case CFUNC_glBeginQueryARB:
    case CFUNC_glEndQueryARB:
        if (!count_fragments) break;
        s = (stats_struct *) data->context_data;
        if (!s->initialised) init_stats_struct(s);
        if (s->query)
        {
            fputs("App is using occlusion queries, disabling fragment counting\n", stderr);
            s->query = 0;
            s->fragments = 0;
        }
        break;
#endif
    }
    return true;
}

static bool stats_post_callback(function_call *call, const callback_data *data)
{
    stats_struct *s;

    switch (canonical_call(call))
    {
    case CFUNC_glXSwapBuffers:
        s = (stats_struct *) data->context_data;
        if (!s->initialised) init_stats_struct(s);
#ifdef GL_ARB_occlusion_query
        if (s->query && begin_internal_render())
        {
            CALL_glBeginQueryARB(GL_SAMPLES_PASSED, s->query);
            end_internal_render("stats_post_callback", true);
        }
#endif
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
    char buffer_fps[128];
    char buffer_fragments[128];
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
            snprintf(buffer_fps, sizeof(buffer_fps), "%.1f fps", s->fps);
            snprintf(buffer_fragments, sizeof(buffer_fragments), "%u fragments", (unsigned int) s->fragments);
            /* We don't want to depend on glWindowPos since it
             * needs OpenGL 1.4, but fortunately the aux context
             * has identity MVP matrix.
             */
            CALL_glPushAttrib(GL_CURRENT_BIT);
            CALL_glRasterPos2f(-0.9, -0.9);
            CALL_glPushAttrib(GL_CURRENT_BIT);
            for (ch = buffer_fps; *ch; ch++)
                CALL_glCallList(s->font_base + *ch);
            CALL_glPopAttrib();
            if (s->query)
            {
                CALL_glBitmap(0, 0, 0, 0, 0, 16, NULL);
                for (ch = buffer_fragments; *ch; ch++)
                    CALL_glCallList(s->font_base + *ch);
            }
            CALL_glPopAttrib();
            glXMakeContextCurrent(dpy, old_write, old_read, real);
            end_internal_render("showstats_callback", true);
        }
        break;
    }
    return true;
}

static bool command_stats(filter_set *handle, const char *name, const char *value)
{
    if (strcmp(name, "fragments") == 0)
    {
        if (strcmp(value, "yes") == 0)
            count_fragments = true;
        else if (strcmp(value, "no") == 0)
            count_fragments = false;
        else
            fprintf(stderr, "illegal fragments value '%s' (should be yes or no)\n", value);
    }
    else
        return false;
    return true;
}

static bool initialise_stats(filter_set *handle)
{
    filter *f;

    stats_handle = handle;
    f = register_filter(handle, "stats", stats_callback);
    register_filter_catches(f, CFUNC_glXSwapBuffers);
#ifdef CFUNC_glBegin
    register_filter_catches(f, CFUNC_glBegin);
    register_filter_catches(f, CFUNC_glEnd);
#endif
    register_filter_depends("invoke", "stats");
#ifdef CFUNC_glBegin
    if (count_fragments)
    {
        f = register_filter(handle, "stats_post", stats_post_callback);
        register_filter_catches(f, CFUNC_glXSwapBuffers);
        register_filter_set_renders("stats");
        register_filter_post_renders("stats_post");
        register_filter_depends("stats_post", "invoke");
    }
#endif
    log_register_filter("stats");
    register_filter_set_uses_state("stats");
    return true;
}

static bool initialise_showstats(filter_set *handle)
{
    filter *f;

    f = register_filter(handle, "showstats", showstats_callback);
    register_filter_depends("showstats", "stats");
    register_filter_depends("invoke", "showstats");
    register_filter_depends("screenshot", "showstats");
    register_filter_catches(f, CFUNC_glXSwapBuffers);
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
        command_stats,
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
