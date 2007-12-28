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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define GL_GLEXT_PROTOTYPES
#include <stdbool.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <bugle/stats.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/tracker.h>
#include <bugle/log.h>
#include <bugle/glutils.h>
#include <budgie/addresses.h>
#include "src/glexts.h"

#ifdef GL_ARB_occlusion_query

typedef struct
{
    GLuint query;
} stats_fragments_struct;

static stats_signal *stats_fragments_fragments;
static object_view stats_fragments_view;

static void stats_fragments_struct_init(const void *key, void *data)
{
    stats_fragments_struct *s;

    s = (stats_fragments_struct *) data;
    if (stats_fragments_fragments->active
        && bugle_gl_has_extension(BUGLE_GL_ARB_occlusion_query)
        && bugle_begin_internal_render())
    {
        CALL(glGenQueriesARB)(1, &s->query);
        if (s->query)
            CALL(glBeginQueryARB)(GL_SAMPLES_PASSED_ARB, s->query);
        bugle_end_internal_render("stats_fragments_struct_initialise", true);
    }
}

static void stats_fragments_struct_clear(void *data)
{
    stats_fragments_struct *s;

    s = (stats_fragments_struct *) data;
    if (s->query)
        CALL(glDeleteQueries)(1, &s->query);
}

static bool stats_fragments_glXSwapBuffers(function_call *call, const callback_data *data)
{
    stats_fragments_struct *s;
    GLuint fragments;

    s = bugle_object_get_current_data(bugle_context_class, stats_fragments_view);
    if (stats_fragments_fragments->active
        && s && s->query && bugle_begin_internal_render())
    {
        CALL(glEndQueryARB)(GL_SAMPLES_PASSED_ARB);
        CALL(glGetQueryObjectuivARB)(s->query, GL_QUERY_RESULT_ARB, &fragments);
        bugle_end_internal_render("stats_fragments_glXSwapBuffers", true);
        bugle_stats_signal_add(stats_fragments_fragments, fragments);
    }
    return true;
}

static bool stats_fragments_post_glXSwapBuffers(function_call *call, const callback_data *data)
{
    stats_fragments_struct *s;

    s = bugle_object_get_current_data(bugle_context_class, stats_fragments_view);
    if (stats_fragments_fragments->active
        && s && s->query && bugle_begin_internal_render())
    {
        CALL(glBeginQueryARB)(GL_SAMPLES_PASSED_ARB, s->query);
        bugle_end_internal_render("stats_fragments_post_glXSwapBuffers", true);
    }
    return true;
}

static bool stats_fragments_query(function_call *call, const callback_data *data)
{
    stats_fragments_struct *s;

    s = bugle_object_get_current_data(bugle_context_class, stats_fragments_view);
    if (stats_fragments_fragments->active
        && s->query)
    {
        bugle_log_printf("stats_fragments", "query", BUGLE_LOG_NOTICE,
                         "Application is using occlusion queries; disabling fragment counting");
        CALL(glEndQueryARB)(GL_SAMPLES_PASSED_ARB);
        CALL(glDeleteQueriesARB)(1, &s->query);
        s->query = 0;
        stats_fragments_fragments->active = false;
    }
    return true;
}

static bool stats_fragments_initialise(filter_set *handle)
{
    filter *f;

    stats_fragments_view = bugle_object_view_new(bugle_context_class,
                                                 stats_fragments_struct_init,
                                                 stats_fragments_struct_clear,
                                                 sizeof(stats_fragments_struct));

    f = bugle_filter_new(handle, "stats_fragments");
    bugle_filter_catches(f, "glXSwapBuffers", false, stats_fragments_glXSwapBuffers);
    bugle_filter_catches(f, "glBeginQueryARB", false, stats_fragments_query);
    bugle_filter_catches(f, "glEndQueryARB", false, stats_fragments_query);
    bugle_filter_order("stats_fragments", "invoke");
    bugle_filter_order("stats_fragments", "stats");

    f = bugle_filter_new(handle, "stats_fragments_post");
    bugle_filter_catches(f, "glXSwapBuffers", false, stats_fragments_post_glXSwapBuffers);
    bugle_filter_order("invoke", "stats_fragments_post");

    stats_fragments_fragments = bugle_stats_signal_new("fragments", NULL, NULL);
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info stats_fragments_info =
    {
        "stats_fragments",
        stats_fragments_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "stats module: fragments that pass the depth test"
    };

    bugle_filter_set_new(&stats_fragments_info);
    bugle_filter_set_depends("stats_fragments", "stats_basic");
    bugle_filter_set_depends("stats_fragments", "trackextensions");
    bugle_filter_set_renders("stats_fragments");
    bugle_filter_set_stats_generator("stats_fragments");
}

#else /* GL_ARB_occlusion_query */

void bugle_initialise_filter_library(void)
{
}

#endif
