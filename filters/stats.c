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
#include <stdarg.h>

typedef struct
{
    /* resources */
    bool initialised;
    bool font_initialised;
    GLuint font_base;
    GLuint query;

    /* Intermediate data for stats */
    struct timeval last_time; /* Last frame - for fps */
    GLsizei begin_mode;       /* For counting triangles in immediate mode */
    GLsizei begin_count;

    /* stats */
    float fps;
    GLuint fragments;
    GLsizei triangles;

    /* Data for showstats, which averages fps */
    struct timeval last_show_time;
    int skip_frames;
    float shown_fps;
} stats_struct;

static filter_set *stats_handle = NULL;
static bool count_fragments = false;
static bool count_triangles = false;

static void init_stats_struct(stats_struct *s)
{
    s->initialised = true;
    s->query = 0;
    s->begin_mode = GL_NONE;
    s->begin_count = 0;
#ifdef GL_ARB_occlusion_query
    if (count_fragments && function_table[FUNC_glGenQueriesARB].real
        && begin_internal_render())
    {
        CALL_glGenQueriesARB(1, &s->query);
        if (s->query)
            CALL_glBeginQueryARB(GL_SAMPLES_PASSED, s->query);
        end_internal_render("init_stats_struct", true);
    }
#endif

    s->fps = 0.0f;
    s->fragments = 0;
    s->triangles = 0;

    s->shown_fps = 0.0f;
}

/* Increments the triangle count for stats struct s according to mode */
static void update_triangles(stats_struct *s, GLenum mode, GLsizei count)
{
    switch (mode)
    {
    case GL_TRIANGLES:
        s->triangles += count / 3;
        break;
    case GL_QUADS:
        s->triangles += count / 4 * 2;
        break;
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUAD_STRIP:
    case GL_POLYGON:
        if (count >= 3)
            s->triangles += count - 2;
        break;
    }
}

static bool stats_callback(function_call *call, const callback_data *data)
{
    stats_struct *s;
    struct timeval now;
    float elapsed;
    FILE *f;
    GLsizei i, primcount;
    budgie_function canon;

    s = (stats_struct *) data->context_data;
    canon = canonical_call(call);
    switch (canon)
    {
    case CFUNC_glXSwapBuffers:
        if (!s->initialised) init_stats_struct(s);
        gettimeofday(&now, NULL);
        elapsed = (now.tv_sec - s->last_time.tv_sec)
            + 1.0e-6f * (now.tv_usec - s->last_time.tv_usec);
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

        if ((f = log_header("stats", "fps")) != NULL)
            fprintf(f, "%.3f\n", s->fps);
        if (s->query && (f = log_header("stats", "fragments")) != NULL)
            fprintf(f, "%u\n", (unsigned int) s->fragments);
        if (count_triangles && (f = log_header("stats", "triangles")) != NULL)
            fprintf(f, "%u\n", (unsigned int) s->triangles);
        break;
    }

#ifdef GL_ARB_occlusion_query
    if (count_fragments)
        switch (canon)
        {
        case CFUNC_glBeginQueryARB:
        case CFUNC_glEndQueryARB:
            if (!s->initialised) init_stats_struct(s);
            if (s->query)
            {
                fputs("App is using occlusion queries, disabling fragment counting\n", stderr);
                s->query = 0;
                s->fragments = 0;
            }
            break;
        }
#endif

    if (count_triangles)
        switch (canon)
        {
#ifdef GL_ARB_vertex_program
        case CFUNC_glVertexAttrib1sARB:
        case CFUNC_glVertexAttrib1fARB:
        case CFUNC_glVertexAttrib1dARB:
        case CFUNC_glVertexAttrib2sARB:
        case CFUNC_glVertexAttrib2fARB:
        case CFUNC_glVertexAttrib2dARB:
        case CFUNC_glVertexAttrib3sARB:
        case CFUNC_glVertexAttrib3fARB:
        case CFUNC_glVertexAttrib3dARB:
        case CFUNC_glVertexAttrib4sARB:
        case CFUNC_glVertexAttrib4fARB:
        case CFUNC_glVertexAttrib4dARB:
        case CFUNC_glVertexAttrib4NubARB:
        case CFUNC_glVertexAttrib1svARB:
        case CFUNC_glVertexAttrib1fvARB:
        case CFUNC_glVertexAttrib1dvARB:
        case CFUNC_glVertexAttrib2svARB:
        case CFUNC_glVertexAttrib2fvARB:
        case CFUNC_glVertexAttrib2dvARB:
        case CFUNC_glVertexAttrib3svARB:
        case CFUNC_glVertexAttrib3fvARB:
        case CFUNC_glVertexAttrib3dvARB:
        case CFUNC_glVertexAttrib4bvARB:
        case CFUNC_glVertexAttrib4svARB:
        case CFUNC_glVertexAttrib4ivARB:
        case CFUNC_glVertexAttrib4ubvARB:
        case CFUNC_glVertexAttrib4usvARB:
        case CFUNC_glVertexAttrib4uivARB:
        case CFUNC_glVertexAttrib4fvARB:
        case CFUNC_glVertexAttrib4dvARB:
        case CFUNC_glVertexAttrib4NbvARB:
        case CFUNC_glVertexAttrib4NsvARB:
        case CFUNC_glVertexAttrib4NivARB:
        case CFUNC_glVertexAttrib4NubvARB:
        case CFUNC_glVertexAttrib4NusvARB:
        case CFUNC_glVertexAttrib4NuivARB:
            if (*(GLuint *) call->generic.args[0] != 0) break;
            /* fall through */
#endif
        case CFUNC_glVertex2d:
        case CFUNC_glVertex2dv:
        case CFUNC_glVertex2f:
        case CFUNC_glVertex2fv:
        case CFUNC_glVertex2i:
        case CFUNC_glVertex2iv:
        case CFUNC_glVertex2s:
        case CFUNC_glVertex2sv:
        case CFUNC_glVertex3d:
        case CFUNC_glVertex3dv:
        case CFUNC_glVertex3f:
        case CFUNC_glVertex3fv:
        case CFUNC_glVertex3i:
        case CFUNC_glVertex3iv:
        case CFUNC_glVertex3s:
        case CFUNC_glVertex3sv:
        case CFUNC_glVertex4d:
        case CFUNC_glVertex4dv:
        case CFUNC_glVertex4f:
        case CFUNC_glVertex4fv:
        case CFUNC_glVertex4i:
        case CFUNC_glVertex4iv:
        case CFUNC_glVertex4s:
        case CFUNC_glVertex4sv:
        case CFUNC_glArrayElement:
            if (!s->initialised) init_stats_struct(s);
            if (in_begin_end()) s->begin_count++;
            break;
        case CFUNC_glBegin:
            if (!s->initialised) init_stats_struct(s);
            s->begin_mode = *call->typed.glBegin.arg0;
            s->begin_count = 0;
            break;
        case CFUNC_glEnd:
            if (!s->initialised) init_stats_struct(s);
            update_triangles(s, s->begin_mode, s->begin_count);
            s->begin_mode = 0;
            s->begin_count = 0;
            break;
        case CFUNC_glDrawArrays:
            if (!s->initialised) init_stats_struct(s);
            update_triangles(s, *call->typed.glDrawArrays.arg0, *call->typed.glDrawArrays.arg2);
            break;
        case CFUNC_glDrawElements:
            update_triangles(s, *call->typed.glDrawElements.arg0, *call->typed.glDrawElements.arg1);
            break;
#ifdef GL_EXT_draw_range_elements
        case CFUNC_glDrawRangeElementsEXT:
            if (!s->initialised) init_stats_struct(s);
            update_triangles(s, *call->typed.glDrawRangeElementsEXT.arg0, *call->typed.glDrawRangeElementsEXT.arg3);
            break;
#endif
#ifdef GL_EXT_multi_draw_arrays
        case CFUNC_glMultiDrawArraysEXT:
            if (!s->initialised) init_stats_struct(s);
            primcount = *call->typed.glMultiDrawArrays.arg3;
            for (i = 0; i < primcount; i++)
                update_triangles(s, *call->typed.glMultiDrawArrays.arg0,
                                 (*call->typed.glMultiDrawArrays.arg2)[i]);
            break;
        case CFUNC_glMultiDrawElementsEXT:
            if (!s->initialised) init_stats_struct(s);
            primcount = *call->typed.glMultiDrawElements.arg4;
            for (i = 0; i < primcount; i++)
                update_triangles(s, *call->typed.glMultiDrawElements.arg0,
                                 (*call->typed.glMultiDrawElements.arg1)[i]);
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
        s->triangles = 0;
        break;
    }
    return true;
}

/* Renders a line of text to screen, and moves down one line */
static void render_stats(stats_struct *s, const char *fmt, ...)
{
    va_list ap;
    char buffer[128];
    char *ch;

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    CALL_glPushAttrib(GL_CURRENT_BIT);
    for (ch = buffer; *ch; ch++)
        CALL_glCallList(s->font_base + *ch);
    CALL_glPopAttrib();
    CALL_glBitmap(0, 0, 0, 0, 0, -16, NULL);
}

static bool showstats_callback(function_call *call, const callback_data *data)
{
    Display *dpy;
    GLXDrawable old_read, old_write;
    GLXContext aux, real;
    Font f;
    stats_struct *s;
    float elapsed;
    struct timeval now;

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
            CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);
            s = get_filter_set_context_state(tracker_get_context_state(), stats_handle);
            if (!s->font_initialised)
            {
                s->font_initialised = true;
                s->font_base = CALL_glGenLists(256);
                f = XLoadFont(dpy, "-*-courier-*-*-*");
                glXUseXFont(f, 0, 256, s->font_base);
                XUnloadFont(dpy, f);
            }

            gettimeofday(&now, NULL);
            elapsed = (now.tv_sec - s->last_show_time.tv_sec)
                + 1.0e-6f * (now.tv_usec - s->last_show_time.tv_usec);
            s->skip_frames++;
            if (elapsed >= 1.0f)
            {
                s->shown_fps = s->skip_frames / elapsed;
                s->last_show_time = now;
                s->skip_frames = 0;
            }

            /* We don't want to depend on glWindowPos since it
             * needs OpenGL 1.4, but fortunately the aux context
             * has identity MVP matrix.
             */
            CALL_glPushAttrib(GL_CURRENT_BIT);
            CALL_glRasterPos2f(-0.9, 0.9);
            render_stats(s, "%.1f fps", s->shown_fps);
            if (s->query) render_stats(s, "%u fragments", (unsigned int) s->fragments);
            if (count_triangles) render_stats(s, "%u triangles", (unsigned int) s->triangles);
            CALL_glPopAttrib();
            CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
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
    else if (strcmp(name, "triangles") == 0)
    {
        if (strcmp(value, "yes") == 0)
            count_triangles = true;
        else if (strcmp(value, "no") == 0)
            count_triangles = false;
        else
            fprintf(stderr, "illegal triangles value '%s' (should be yes or no)\n", value);
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
    if (count_triangles)
    {
#ifdef GL_ARB_vertex_program
        register_filter_catches(f, CFUNC_glVertexAttrib1sARB);
        register_filter_catches(f, CFUNC_glVertexAttrib1fARB);
        register_filter_catches(f, CFUNC_glVertexAttrib1dARB);
        register_filter_catches(f, CFUNC_glVertexAttrib2sARB);
        register_filter_catches(f, CFUNC_glVertexAttrib2fARB);
        register_filter_catches(f, CFUNC_glVertexAttrib2dARB);
        register_filter_catches(f, CFUNC_glVertexAttrib3sARB);
        register_filter_catches(f, CFUNC_glVertexAttrib3fARB);
        register_filter_catches(f, CFUNC_glVertexAttrib3dARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4sARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4fARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4dARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4NubARB);
        register_filter_catches(f, CFUNC_glVertexAttrib1svARB);
        register_filter_catches(f, CFUNC_glVertexAttrib1fvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib1dvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib2svARB);
        register_filter_catches(f, CFUNC_glVertexAttrib2fvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib2dvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib3svARB);
        register_filter_catches(f, CFUNC_glVertexAttrib3fvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib3dvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4bvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4svARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4ivARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4ubvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4usvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4uivARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4fvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4dvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4NbvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4NsvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4NivARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4NubvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4NusvARB);
        register_filter_catches(f, CFUNC_glVertexAttrib4NuivARB);
#endif
        register_filter_catches(f, CFUNC_glVertex2d);
        register_filter_catches(f, CFUNC_glVertex2dv);
        register_filter_catches(f, CFUNC_glVertex2f);
        register_filter_catches(f, CFUNC_glVertex2fv);
        register_filter_catches(f, CFUNC_glVertex2i);
        register_filter_catches(f, CFUNC_glVertex2iv);
        register_filter_catches(f, CFUNC_glVertex2s);
        register_filter_catches(f, CFUNC_glVertex2sv);
        register_filter_catches(f, CFUNC_glVertex3d);
        register_filter_catches(f, CFUNC_glVertex3dv);
        register_filter_catches(f, CFUNC_glVertex3f);
        register_filter_catches(f, CFUNC_glVertex3fv);
        register_filter_catches(f, CFUNC_glVertex3i);
        register_filter_catches(f, CFUNC_glVertex3iv);
        register_filter_catches(f, CFUNC_glVertex3s);
        register_filter_catches(f, CFUNC_glVertex3sv);
        register_filter_catches(f, CFUNC_glVertex4d);
        register_filter_catches(f, CFUNC_glVertex4dv);
        register_filter_catches(f, CFUNC_glVertex4f);
        register_filter_catches(f, CFUNC_glVertex4fv);
        register_filter_catches(f, CFUNC_glVertex4i);
        register_filter_catches(f, CFUNC_glVertex4iv);
        register_filter_catches(f, CFUNC_glVertex4s);
        register_filter_catches(f, CFUNC_glVertex4sv);
        register_filter_catches(f, CFUNC_glArrayElement);
        register_filter_catches(f, CFUNC_glBegin);
        register_filter_catches(f, CFUNC_glEnd);
        register_filter_catches(f, CFUNC_glDrawElements);
        register_filter_catches(f, CFUNC_glDrawArrays);
#ifdef GL_EXT_draw_range_elements
        register_filter_catches(f, CFUNC_glDrawRangeElementsEXT);
#endif
#ifdef GL_EXT_multi_draw_arrays
        register_filter_catches(f, CFUNC_glMultiDrawElementsEXT);
        register_filter_catches(f, CFUNC_glMultiDrawArraysEXT);
#endif
    }
    register_filter_depends("invoke", "stats");
    if (count_fragments || count_triangles)
    {
        f = register_filter(handle, "stats_post", stats_post_callback);
        register_filter_catches(f, CFUNC_glXSwapBuffers);
        register_filter_set_renders("stats");
        register_filter_post_renders("stats_post");
        register_filter_depends("stats_post", "invoke");
    }
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
    /* make sure that screenshots capture the stats */
    register_filter_depends("debugger", "showstats");
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
