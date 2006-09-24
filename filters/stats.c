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

/* Raw statistics come in three flavours:
 * - accumulating: a count that only goes up over time, such as frames,
 *   seconds, triangles etc
 * - continuous: a value that may go up and down over time, but is not
 *   a rate e.g. memory used or GPU utilisation
 *
 * Things that are displayed are called 'samplers'. A sampler measures
 * something over a particular period of time. There are three types of
 * sampler:
 * - rate: the ratio of the change in two accumulating statistics over
 *   the sampling period
 * - continuous: the average value over the sampling period. The value is
 *   sampled at the end of each frame, but the samples are weighted by
 *   time.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _XOPEN_SOURCE 500
#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/tracker.h"
#include "src/log.h"
#include "src/glexts.h"
#include "src/xevent.h"
#include "common/safemem.h"
#include "common/bool.h"
#include "common/linkedlist.h"
#include <stdio.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <sys/time.h>
#include <stdarg.h>
#include <math.h>
#include <ltdl.h>

#define ENABLE_STATS_NV 1 /* FIXME */

typedef enum
{
    STATISTIC_ACCUMULATING,
    STATISTIC_CONTINUOUS,
} statistic_type;

typedef enum
{
    SAMPLER_RATE,
    SAMPLER_CONTINUOUS,
} sampler_type;

/* Child classes that need extra data should allocate a larger structure
 * that contains this one. char * fields are malloc'ed.
 *
 * A statistic may be set to 'uninitialised' after it is initialised. This
 * indicates that it has failed for some reason, and samplers based on it
 * should be disabled.
 */
typedef struct statistic_s
{
    bool initialised;
    statistic_type type;
    char *name;                /* short name (for internal use) - malloc'ed */
    double value;
    void *user_data;

    /* Called if the stat is used. May be NULL if no initialisation is needed */
    bool (*initialise)(struct statistic_s *);
} statistic;

typedef struct sampler_s
{
    sampler_type type;
    char *name;                /* name for config file */
    char *format;              /* For printf - should have one %f */

    /* arrays of size 2 contain info for numerator and denominator when
     * a rate, or data only in element 0 for the other types.
     */
    statistic *basis[2];
    double first_value[2], last_value[2];
    struct timeval first_time, last_time;

    double value;
} sampler;

static bugle_linked_list registered_statistics;
static bugle_linked_list registered_samplers;
static bugle_linked_list active_samplers;

statistic *bugle_register_statistic(statistic_type type,
                                    const char *name,
                                    void *user_data,
                                    bool (*initialise)(struct statistic_s *))
{
    statistic *st;

    st = bugle_malloc(sizeof(statistic));
    st->initialised = false;
    st->type = type;
    st->name = bugle_strdup(name);
    st->value = 0.0;
    st->user_data = user_data;
    st->initialise = initialise;

    bugle_list_append(&registered_statistics, st);
    return st;
}

/* Searches for a registered statistic with the given name, returning
 * it if found or NULL if not.
 */
statistic *bugle_statistic_find(const char *name)
{
    bugle_list_node *i;
    statistic *cur;

    for (i = bugle_list_head(&registered_statistics); i; i = bugle_list_next(i))
    {
        cur = (statistic *) bugle_list_data(i);
        if (strcmp(cur->name, name) == 0) return cur;
    }
    return NULL;
}

sampler *bugle_register_sampler_rate(const char *name,
                                     const char *numerator,
                                     const char *denominator,
                                     const char *format)
{
    sampler *sa;
    statistic *n, *d;

    n = bugle_statistic_find(numerator);
    d = bugle_statistic_find(denominator);
    if (n && d)
    {
        sa = bugle_calloc(1, sizeof(sampler));
        sa->type = SAMPLER_RATE;
        sa->name = bugle_strdup(name);
        sa->format = bugle_strdup(format);
        sa->basis[0] = n;
        sa->basis[1] = d;

        bugle_list_append(&registered_samplers, sa);
        return sa;
    }
    else
        return NULL;
}

sampler *bugle_register_sampler_continuous(const char *name,
                                           const char *stat,
                                           const char *format)
{
    sampler *sa;
    statistic *st;

    st = bugle_statistic_find(stat);
    if (st)
    {
        sa = bugle_calloc(1, sizeof(sampler));
        sa->type = SAMPLER_CONTINUOUS;
        sa->name = bugle_strdup(name);
        sa->format = bugle_strdup(format);
        sa->basis[0] = st;

        bugle_list_append(&registered_samplers, sa);
        return sa;
    }
    else
        return NULL;
}

/* Like bugle_statistic_find, but for samplers. It looks through registered
 * rather than active samplers.
 */
sampler *bugle_sampler_find(const char *name)
{
    bugle_list_node *i;
    sampler *cur;

    for (i = bugle_list_head(&registered_samplers); i; i = bugle_list_next(i))
    {
        cur = (sampler *) bugle_list_data(i);
        if (strcmp(cur->name, name) == 0) return cur;
    }
    return NULL;
}

/*** Utilities ***/

static void sampler_initialise(sampler *sa)
{
    int j;

    for (j = 0; j < 2; j++)
        if (sa->basis[j] && !sa->basis[j]->initialised)
        {
            sa->basis[j]->initialised = true;
            if (sa->basis[j]->initialise)
                (*sa->basis[j]->initialise)(sa->basis[j]);
        }

    switch (sa->type)
    {
    case SAMPLER_RATE:
        sa->last_value[0] = sa->first_value[0] = sa->basis[0]->value;
        sa->last_value[1] = sa->first_value[1] = sa->basis[1]->value;
        break;
    case SAMPLER_CONTINUOUS:
        sa->last_value[0] = sa->first_value[0] = 0.0;
        break;
    }
    gettimeofday(&sa->first_time, NULL);
    sa->last_time = sa->first_time;
    sa->value = 0.0;
}

/* Moves the first_time forward to match the last_time. It does not
 * reset the value field (see below for explanation) */
static void sampler_reset(sampler *sa)
{
    switch (sa->type)
    {
    case SAMPLER_RATE:
        sa->first_value[0] = sa->last_value[0];
        sa->first_value[1] = sa->last_value[1];
        break;
    case SAMPLER_CONTINUOUS:
        sa->first_value[0] = sa->last_value[0] = 0.0;
        sa->first_time = sa->last_time;
        break;
    }
}

/* Returns the time between first and last in the sampler */
static double sampler_elapsed(sampler *sa)
{
    return (sa->last_time.tv_sec - sa->first_time.tv_sec)
        + 1e-6 * (sa->last_time.tv_usec - sa->first_time.tv_usec);
}

/* Updates the internal statistics, but does *not* update the value field.
 * This allows continuous variables to be sampled with arbitrary
 * granularity, while updating the displayed value on a different
 * timescale.
 */
static void sampler_update(sampler *sa)
{
    double elapsed;
    struct timeval now;

    switch (sa->type)
    {
    case SAMPLER_RATE:
        sa->last_value[0] = sa->basis[0]->value;
        sa->last_value[1] = sa->basis[1]->value;
        break;
    case SAMPLER_CONTINUOUS:
        /* Update the time-integral */
        gettimeofday(&now, NULL);
        elapsed = (now.tv_sec - sa->last_time.tv_sec)
            + 1e-6 * (now.tv_usec - sa->last_time.tv_usec);
        sa->last_value[0] += elapsed * sa->basis[0]->value;
        sa->last_time = now;
        break;
    }
}

/* Updates the value in a sampler, without updating the sampler itself */
static double sampler_update_value(sampler *sa)
{
    switch (sa->type)
    {
    case SAMPLER_RATE:
        if (!sa->basis[0]->initialised || !sa->basis[1]->initialised)
            return sa->value = HUGE_VAL;
        else
            return sa->value = (sa->last_value[0] - sa->first_value[0])
                / (sa->last_value[1] - sa->first_value[1]);
    case SAMPLER_CONTINUOUS:
        if (!sa->basis[0]->initialised)
            return sa->value = HUGE_VAL;
        else
            return sa->value = (sa->last_value[0] - sa->first_value[0])
                / sampler_elapsed(sa);
    }
    return 0.0; /* Should never be reached */
}

static void bugle_sampler_activate(sampler *sa)
{
    bugle_list_node *i;
    sampler *cur;

    for (i = bugle_list_head(&active_samplers); i; i = bugle_list_next(i))
    {
        cur = (sampler *) bugle_list_data(i);
        if (cur == sa) return;  /* already selected */
    }
    bugle_list_append(&active_samplers, sa);
    sampler_initialise(sa);
}

/*** 'stats' filter-set: driver code ***/

static bool initialise_stats(filter_set *handle)
{
    bugle_list_init(&registered_statistics, true);
    bugle_list_init(&registered_samplers, true);
    bugle_list_init(&active_samplers, false);

    /* This filter does no interception, but it provides a sequence point
     * to allow display modules (showstats, logstats) to occur after
     * statistics modules (stats_basic etc).
     */
    bugle_register_filter(handle, "stats");
    return true;
}

static void destroy_stats(filter_set *handle)
{
    bugle_list_clear(&registered_statistics);
    bugle_list_clear(&registered_samplers);
    bugle_list_clear(&active_samplers);
}

/*** Default statistics ***/

static statistic *stats_basic_frames, *stats_basic_seconds, *stats_basic_milliseconds;

static bool stats_basic_initialise_seconds(statistic *st)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    st->value = t.tv_sec + 1e-6 * t.tv_usec;
    return true;
}

static bool stats_basic_initialise_milliseconds(statistic *st)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    st->value = 1e3 * t.tv_sec + 1e-3 * t.tv_usec;
    return true;
}

static bool stats_basic_glXSwapBuffers(function_call *call, const callback_data *data)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    stats_basic_seconds->value = now.tv_sec + 1e-6 * now.tv_usec;
    stats_basic_milliseconds->value = 1e3 * now.tv_sec + 1e-3 * now.tv_usec;
    stats_basic_frames->value++;
    return true;
}

static bool initialise_stats_basic(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "stats_basic");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, stats_basic_glXSwapBuffers);
    bugle_register_filter_depends("invoke", "stats_basic");
    bugle_register_filter_depends("stats", "stats_basic");

    stats_basic_frames = bugle_register_statistic(STATISTIC_ACCUMULATING, "frames", NULL, NULL);
    stats_basic_seconds = bugle_register_statistic(STATISTIC_ACCUMULATING, "seconds", NULL, stats_basic_initialise_seconds);
    stats_basic_milliseconds = bugle_register_statistic(STATISTIC_ACCUMULATING, "milliseconds", NULL, stats_basic_initialise_milliseconds);
    bugle_register_sampler_rate("frame_rate", "frames", "seconds", "frame rate: %.2ffps");
    bugle_register_sampler_rate("frame_time", "milliseconds", "frames", "frame time: %.2fms");
    return true;
}

/*** Primitive counts ***/

static bugle_object_view stats_primitives_view;  /* begin/end counting */
static bugle_object_view stats_primitives_displaylist_view;
static statistic *stats_primitives_batches, *stats_primitives_triangles;

typedef struct
{
    GLenum begin_mode;        /* For counting triangles in immediate mode */
    GLsizei begin_count;      /* FIXME: use per-begin/end object */
} stats_primitives_struct;

typedef struct
{
    GLsizei triangles;
    GLsizei batches;
} stats_primitives_displaylist_struct;

/* Increments the triangle count according to mode */
static void update_triangles(GLenum mode, GLsizei count)
{
    size_t t = 0;
    stats_primitives_displaylist_struct *displaylist;

    switch (mode)
    {
    case GL_TRIANGLES:
        t = count / 3;
        break;
    case GL_QUADS:
        t = count / 4 * 2;
        break;
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUAD_STRIP:
    case GL_POLYGON:
        if (count >= 3)
            t = count - 2;
        break;
    }
    if (!t) return;

    switch (bugle_displaylist_mode())
    {
    case GL_NONE:
        stats_primitives_triangles->value += t;
        stats_primitives_batches->value++;
        break;
    case GL_COMPILE_AND_EXECUTE:
        stats_primitives_triangles->value += t;
        stats_primitives_batches->value++;
        /* Fall through */
    case GL_COMPILE:
        displaylist = bugle_object_get_current_data(&bugle_displaylist_class, stats_primitives_displaylist_view);
        assert(displaylist);
        displaylist->triangles += t;
        displaylist->batches++;
        break;
    default:
        abort();
    }
}

static bool stats_primitives_immediate(function_call *call, const callback_data *data)
{
    stats_primitives_struct *s;

    if (bugle_in_begin_end())
    {
        s = bugle_object_get_current_data(&bugle_context_class, stats_primitives_view);
        s->begin_count++;
    }
    return true;
}

static bool stats_primitives_glBegin(function_call *call, const callback_data *data)
{
    stats_primitives_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_primitives_view);
    s->begin_mode = *call->typed.glBegin.arg0;
    s->begin_count = 0;
    return true;
}

static bool stats_primitives_glEnd(function_call *call, const callback_data *data)
{
    stats_primitives_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_primitives_view);
    update_triangles(s->begin_mode, s->begin_count);
    s->begin_mode = GL_NONE;
    s->begin_count = 0;
    return true;
}

static bool stats_primitives_glDrawArrays(function_call *call, const callback_data *data)
{
    update_triangles(*call->typed.glDrawArrays.arg0, *call->typed.glDrawArrays.arg2);
    return true;
}

static bool stats_primitives_glDrawElements(function_call *call, const callback_data *data)
{
    update_triangles(*call->typed.glDrawElements.arg0, *call->typed.glDrawElements.arg1);
    return true;
}

#ifdef GL_EXT_draw_range_elements
static bool stats_primitives_glDrawRangeElements(function_call *call, const callback_data *data)
{
    update_triangles(*call->typed.glDrawRangeElementsEXT.arg0, *call->typed.glDrawRangeElementsEXT.arg3);
    return true;
}
#endif

#ifdef GL_EXT_multi_draw_arrays
static bool stats_primitives_glMultiDrawArrays(function_call *call, const callback_data *data)
{
    GLsizei i, primcount;

    primcount = *call->typed.glMultiDrawArrays.arg3;
    for (i = 0; i < primcount; i++)
        update_triangles(*call->typed.glMultiDrawArrays.arg0,
                         (*call->typed.glMultiDrawArrays.arg2)[i]);
    return true;
}

static bool stats_primitives_glMultiDrawElements(function_call *call, const callback_data *data)
{
    GLsizei i, primcount;

    primcount = *call->typed.glMultiDrawElements.arg4;
    for (i = 0; i < primcount; i++)
        update_triangles(*call->typed.glMultiDrawElements.arg0,
                         (*call->typed.glMultiDrawElements.arg1)[i]);
    return true;
}
#endif

static bool stats_primitives_glCallList(function_call *call, const callback_data *data)
{
    stats_primitives_struct *s;
    stats_primitives_displaylist_struct *counts;

    s = bugle_object_get_current_data(&bugle_context_class, stats_primitives_view);
    counts = bugle_object_get_data(&bugle_displaylist_class,
                                   bugle_displaylist_get(*call->typed.glCallList.arg0),
                                   stats_primitives_displaylist_view);
    if (counts)
    {
        stats_primitives_triangles->value += counts->triangles;
        stats_primitives_batches->value += counts->batches;
    }
    return true;
}

static bool stats_primitives_glCallLists(function_call *call, const callback_data *data)
{
    fputs("FIXME: triangle counting in glCallLists not implemented!\n", stderr);
    return true;
}

static bool initialise_stats_primitives(filter_set *handle)
{
    filter *f;

    stats_primitives_view = bugle_object_class_register(&bugle_context_class,
                                                        NULL,
                                                        NULL,
                                                        sizeof(stats_primitives_struct));
    stats_primitives_displaylist_view = bugle_object_class_register(&bugle_displaylist_class,
                                                                    NULL,
                                                                    NULL,
                                                                    sizeof(size_t));

    f = bugle_register_filter(handle, "stats_primitives");
    bugle_register_filter_catches_drawing_immediate(f, false, stats_primitives_immediate);
    bugle_register_filter_catches(f, GROUP_glDrawElements, false, stats_primitives_glDrawElements);
    bugle_register_filter_catches(f, GROUP_glDrawArrays, false, stats_primitives_glDrawArrays);
#ifdef GL_EXT_draw_range_elements
    bugle_register_filter_catches(f, GROUP_glDrawRangeElementsEXT, false, stats_primitives_glDrawRangeElements);
#endif
#ifdef GL_EXT_multi_draw_arrays
    bugle_register_filter_catches(f, GROUP_glMultiDrawElementsEXT, false, stats_primitives_glMultiDrawElements);
    bugle_register_filter_catches(f, GROUP_glMultiDrawArraysEXT, false, stats_primitives_glMultiDrawArrays);
#endif
    bugle_register_filter_catches(f, GROUP_glBegin, false, stats_primitives_glBegin);
    bugle_register_filter_catches(f, GROUP_glEnd, false, stats_primitives_glEnd);
    bugle_register_filter_catches(f, GROUP_glCallList, false, stats_primitives_glCallList);
    bugle_register_filter_catches(f, GROUP_glCallLists, false, stats_primitives_glCallLists);
    bugle_register_filter_depends("invoke", "stats_primitives");
    bugle_register_filter_depends("stats", "stats_primitives");

    stats_primitives_batches = bugle_register_statistic(STATISTIC_ACCUMULATING, "batches", NULL, NULL);
    stats_primitives_triangles = bugle_register_statistic(STATISTIC_ACCUMULATING, "triangles", NULL, NULL);
    bugle_register_sampler_rate("triangles", "triangles", "frames", "triangles: %.0f");
    bugle_register_sampler_rate("triangle_rate", "triangles", "seconds", "triangle rate: %.1f/s");
    bugle_register_sampler_rate("batches", "batches", "frames", "batches: %.0f");
    bugle_register_sampler_rate("batch_rate", "batches", "seconds", "batch rate: %.1f/s");

    return true;
}

/*** Fragment counts ***/

#ifdef GL_ARB_occlusion_query

typedef struct
{
    GLuint query;
} stats_fragments_struct;

static statistic *stats_fragments_fragments;
static bugle_object_view stats_fragments_view;

static void initialise_stats_fragments_struct(const void *key, void *data)
{
    stats_fragments_struct *s;

    s = (stats_fragments_struct *) data;
    if (stats_fragments_fragments->initialised
        && bugle_gl_has_extension(BUGLE_GL_ARB_occlusion_query)
        && bugle_begin_internal_render())
    {
        CALL_glGenQueriesARB(1, &s->query);
        if (s->query)
            CALL_glBeginQueryARB(GL_SAMPLES_PASSED_ARB, s->query);
        bugle_end_internal_render("initialise_stats_fragments_struct", true);
    }
}

static void destroy_stats_fragments_struct(void *data)
{
    stats_fragments_struct *s;

    s = (stats_fragments_struct *) data;
    if (s->query)
        CALL_glDeleteQueries(1, &s->query);
}

static bool stats_fragments_glXSwapBuffers(function_call *call, const callback_data *data)
{
    stats_fragments_struct *s;
    GLuint fragments;

    s = bugle_object_get_current_data(&bugle_context_class, stats_fragments_view);
    if (stats_fragments_fragments->initialised
        && s && s->query && bugle_begin_internal_render())
    {
        CALL_glEndQueryARB(GL_SAMPLES_PASSED_ARB);
        CALL_glGetQueryObjectuivARB(s->query, GL_QUERY_RESULT_ARB, &fragments);
        bugle_end_internal_render("stats_fragments_glXSwapBuffers", true);
        stats_fragments_fragments->value += fragments;
    }
    return true;
}

static bool stats_fragments_post_glXSwapBuffers(function_call *call, const callback_data *data)
{
    stats_fragments_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_fragments_view);
    if (stats_fragments_fragments->initialised
        && s && s->query && bugle_begin_internal_render())
    {
        CALL_glBeginQueryARB(GL_SAMPLES_PASSED_ARB, s->query);
        bugle_end_internal_render("stats_fragments_post_glXSwapBuffers", true);
    }
    return true;
}

static bool stats_fragments_query(function_call *call, const callback_data *data)
{
    stats_fragments_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_fragments_view);
    if (stats_fragments_fragments->initialised
        && s->query)
    {
        fputs("App is using occlusion queries, disabling fragment counting\n", stderr);
        CALL_glEndQueryARB(GL_SAMPLES_PASSED_ARB);
        CALL_glDeleteQueriesARB(1, &s->query);
        s->query = 0;
        stats_fragments_fragments->initialised = false;
    }
    return true;
}

static bool initialise_stats_fragments(filter_set *handle)
{
    filter *f;

    stats_fragments_view = bugle_object_class_register(&bugle_context_class,
                                                       initialise_stats_fragments_struct,
                                                       destroy_stats_fragments_struct,
                                                       sizeof(stats_fragments_struct));

    f = bugle_register_filter(handle, "stats_fragments");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, stats_fragments_glXSwapBuffers);
    bugle_register_filter_catches(f, GROUP_glBeginQueryARB, false, stats_fragments_query);
    bugle_register_filter_catches(f, GROUP_glEndQueryARB, false, stats_fragments_query);
    bugle_register_filter_depends("invoke", "stats_fragments");
    bugle_register_filter_depends("stats", "stats_fragments");

    f = bugle_register_filter(handle, "stats_fragments_post");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, stats_fragments_post_glXSwapBuffers);
    bugle_register_filter_depends("stats_fragments_post", "invoke");

    stats_fragments_fragments = bugle_register_statistic(STATISTIC_ACCUMULATING, "fragments", NULL, NULL);
    bugle_register_sampler_rate("fragments", "fragments", "frames", "fragments: %.0f");
    bugle_register_sampler_rate("fragment_rate", "fragments", "seconds", "fragment rate: %.1f/s");
    return true;
}
#endif /* GL_ARB_occlusion_query */

/*** NV stats ***/

#if ENABLE_STATS_NV
#include <NVPerfSDK.h>

typedef enum
{
    STATISTIC_NV_RAW,
    STATISTIC_NV_CYCLES,
    STATISTIC_NV_CONTINUOUS
} statistic_nv_type;

typedef struct
{
    UINT index;
    statistic_nv_type type;
} statistic_nv;

static bugle_linked_list stats_nv_active;
static bugle_linked_list stats_nv_registered;
static lt_dlhandle stats_nv_dl = NULL;

static NVPMRESULT (*fNVPMInit)(void);
static NVPMRESULT (*fNVPMShutdown)(void);
static NVPMRESULT (*fNVPMEnumCounters)(NVPMEnumFunc);
static NVPMRESULT (*fNVPMSample)(NVPMSampleValue *, UINT *);
static NVPMRESULT (*fNVPMAddCounter)(UINT);
static NVPMRESULT (*fNVPMRemoveAllCounters)(void);
static NVPMRESULT (*fNVPMGetCounterValue)(UINT, int, UINT64 *, UINT64 *);
static NVPMRESULT (*fNVPMGetCounterAttribute)(UINT, NVPMATTRIBUTE, UINT64 *);

static bool stats_nv_glXSwapBuffers(function_call *call, const callback_data *data)
{
    bugle_list_node *i;
    statistic *st;
    statistic_nv *nv;
    UINT samples;
    UINT64 value, cycles;

    if (bugle_list_head(&stats_nv_active))
        fNVPMSample(NULL, &samples);
    for (i = bugle_list_head(&stats_nv_active); i; i = bugle_list_next(i))
    {
        st = (statistic *) bugle_list_data(i);
        nv = (statistic_nv *) st->user_data;

        fNVPMGetCounterValue(nv->index, 0, &value, &cycles);
        switch (nv->type)
        {
        case STATISTIC_NV_RAW:
            st->value += value;
            break;
        case STATISTIC_NV_CYCLES:
            st->value += cycles;
            break;
        case STATISTIC_NV_CONTINUOUS:
            st->value = value;
            break;
        }
    }
    return true;
}

static bool stats_nv_statistic_initialise(statistic *st)
{
    statistic_nv *nv;

    nv = (statistic_nv *) st->user_data;
    if (fNVPMAddCounter(nv->index) != NVPM_OK) return false;
    bugle_list_append(&stats_nv_active, st);
    return true;
}

static int stats_nv_enumerate(UINT index, char *name)
{
    char *value_name, *cycles_name, *sampler_name, *format;
    statistic *st;
    statistic_nv *nv;
    UINT64 cv_attr, cdh_attr;

    bugle_asprintf(&sampler_name, "nv:%s", name);
    bugle_asprintf(&format, "%s: %%f", name);
    fNVPMGetCounterAttribute(index, NVPMA_COUNTER_VALUE, &cv_attr);
    fNVPMGetCounterAttribute(index, NVPMA_COUNTER_DISPLAY_HINT, &cdh_attr);
    if (cdh_attr == NVPM_CDH_PERCENT)
    {
        bugle_asprintf(&value_name, "nv_value:%s", name);
        bugle_asprintf(&cycles_name, "nv_cycles:%s", name);

        nv = bugle_malloc(sizeof(statistic_nv));
        nv->index = index;
        nv->type = STATISTIC_NV_RAW;
        st = bugle_register_statistic(STATISTIC_ACCUMULATING,
                                      value_name,
                                      nv,
                                      stats_nv_statistic_initialise);
        bugle_list_append(&stats_nv_registered, st);

        nv = bugle_malloc(sizeof(statistic_nv));
        nv->index = index;
        nv->type = STATISTIC_NV_CYCLES;
        st = bugle_register_statistic(STATISTIC_ACCUMULATING,
                                      cycles_name,
                                      nv,
                                      stats_nv_statistic_initialise);
        bugle_list_append(&stats_nv_registered, st);

        bugle_register_sampler_rate(sampler_name, value_name, cycles_name, format);
        free(value_name);
        free(cycles_name);
    }
    else
    {
        nv = bugle_malloc(sizeof(statistic_nv));
        nv->index = index;
        nv->type = STATISTIC_NV_CONTINUOUS;
        st = bugle_register_statistic(STATISTIC_CONTINUOUS,
                                      sampler_name,
                                      nv,
                                      stats_nv_statistic_initialise);
        bugle_list_append(&stats_nv_registered, st);

        bugle_register_sampler_continuous(sampler_name, sampler_name, format);
    }
    free(sampler_name);
    free(format);
    return NVPM_OK;
}

static bool initialise_stats_nv(filter_set *handle)
{
    filter *f;

    bugle_list_init(&stats_nv_active, false);
    bugle_list_init(&stats_nv_registered, false);
    stats_nv_dl = lt_dlopenext("libNVPerfSDK");
    if (stats_nv_dl == NULL) return true;

    fNVPMInit = (NVPMRESULT (*)(void)) lt_dlsym(stats_nv_dl, "NVPMInit");
    fNVPMShutdown = (NVPMRESULT (*)(void)) lt_dlsym(stats_nv_dl, "NVPMShutdown");
    fNVPMEnumCounters = (NVPMRESULT (*)(NVPMEnumFunc)) lt_dlsym(stats_nv_dl, "NVPMEnumCounters");
    fNVPMSample = (NVPMRESULT (*)(NVPMSampleValue *, UINT *)) lt_dlsym(stats_nv_dl, "NVPMSample");
    fNVPMRemoveAllCounters = (NVPMRESULT (*)(void)) lt_dlsym(stats_nv_dl, "NVPMRemoveAllCounters");
    fNVPMAddCounter = (NVPMRESULT (*)(UINT)) lt_dlsym(stats_nv_dl, "NVPMAddCounter");
    fNVPMGetCounterValue = (NVPMRESULT (*)(UINT, int, UINT64 *, UINT64 *)) lt_dlsym(stats_nv_dl, "NVPMGetCounterValue");
    fNVPMGetCounterAttribute = (NVPMRESULT (*)(UINT, NVPMATTRIBUTE, UINT64 *)) lt_dlsym(stats_nv_dl, "NVPMGetCounterAttribute");

    if (!fNVPMInit
        || !fNVPMShutdown
        || !fNVPMEnumCounters
        || !fNVPMSample
        || !fNVPMAddCounter
        || !fNVPMRemoveAllCounters
        || !fNVPMGetCounterValue
        || !fNVPMGetCounterAttribute)
    {
        fputs("Failed to load symbols for NVPerfSDK\n", stderr);
        goto cancel1;
    }

    if (fNVPMInit() != NVPM_OK)
    {
        fputs("Failed to initialise NVPM\n", stderr);
        goto cancel1;
    }
    if (fNVPMEnumCounters(stats_nv_enumerate) != NVPM_OK)
    {
        fputs("Failed to enumerate NVPM counters\n", stderr);
        goto cancel2;
    }

    f = bugle_register_filter(handle, "stats_nv");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, stats_nv_glXSwapBuffers);
    bugle_register_filter_depends("invoke", "stats_nv");
    bugle_register_filter_depends("stats", "stats_nv");
    return true;

cancel2:
    fNVPMShutdown();
cancel1:
    lt_dlclose(stats_nv_dl);
    stats_nv_dl = NULL;
    return false;
}

static void destroy_stats_nv(filter_set *handle)
{
    bugle_list_node *i;
    statistic *st;

    for (i = bugle_list_head(&stats_nv_registered); i; i = bugle_list_next(i))
    {
        st = (statistic *) bugle_list_data(i);
        free(st->user_data);
    }
    fNVPMRemoveAllCounters();
    fNVPMShutdown();
    lt_dlclose(stats_nv_dl);
    bugle_list_clear(&stats_nv_registered);
    bugle_list_clear(&stats_nv_active);
}

#endif /* ENABLE_STATS_NV */

/*** Showstats ***/

typedef struct
{
    GLuint font_base;
    struct timeval last_show_time;
    int accumulating;  /* 0: no  1: yes  2: yes, reset counters */
} showstats_struct;

static bugle_object_view showstats_view;
static bugle_linked_list showstats_requested;
static xevent_key key_showstats_accumulate = { NoSymbol, 0, true };
static xevent_key key_showstats_noaccumulate = { NoSymbol, 0, true };

/* Renders a line of text to screen, and moves down one line */
static void render_stats(showstats_struct *ss, const char *fmt, ...)
{
    va_list ap;
    char buffer[128];
    char *ch;

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    CALL_glPushAttrib(GL_CURRENT_BIT);
    for (ch = buffer; *ch; ch++)
        CALL_glCallList(ss->font_base + *ch);
    CALL_glPopAttrib();
    CALL_glBitmap(0, 0, 0, 0, 0, -16, NULL);
}

static void initialise_showstats_struct(const void *key, void *data)
{
    Display *dpy;
    Font f;
    showstats_struct *ss;

    ss = (showstats_struct *) data;
    dpy = CALL_glXGetCurrentDisplay();
    ss->font_base = CALL_glGenLists(256);
    f = XLoadFont(dpy, "-*-courier-medium-r-normal--10-*");
    CALL_glXUseXFont(f, 0, 256, ss->font_base);
    XUnloadFont(dpy, f);

    ss->last_show_time.tv_sec = 0;
    ss->last_show_time.tv_usec = 0;
    ss->accumulating = 0;
}

static void destroy_showstats_struct(void *data)
{
    showstats_struct *ss;
    ss = (showstats_struct *) data;
    CALL_glDeleteLists(1, ss->font_base);
}

static bool showstats_callback(function_call *call, const callback_data *data)
{
    Display *dpy;
    GLXDrawable old_read, old_write;
    GLXContext aux, real;
    showstats_struct *ss;
    double elapsed;
    struct timeval now;
    GLint viewport[4];
    bugle_list_node *i;
    sampler *sa;

    for (i = bugle_list_head(&active_samplers); i; i = bugle_list_next(i))
    {
        sa = (sampler *) bugle_list_data(i);
        sampler_update(sa);
    }

    aux = bugle_get_aux_context();
    if (aux && bugle_begin_internal_render())
    {
        CALL_glGetIntegerv(GL_VIEWPORT, viewport);
        real = CALL_glXGetCurrentContext();
        old_write = CALL_glXGetCurrentDrawable();
        old_read = CALL_glXGetCurrentReadDrawable();
        dpy = CALL_glXGetCurrentDisplay();
        CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);
        ss = bugle_object_get_current_data(&bugle_context_class, showstats_view);

        gettimeofday(&now, NULL);
        elapsed = (now.tv_sec - ss->last_show_time.tv_sec)
            + 1.0e-6 * (now.tv_usec - ss->last_show_time.tv_usec);
        if (elapsed >= 0.2)
        {
            ss->last_show_time = now;
            for (i = bugle_list_head(&active_samplers); i; i = bugle_list_next(i))
            {
                sa = (sampler *) bugle_list_data(i);
                sampler_update_value(sa);
                if (ss->accumulating != 1)
                    sampler_reset(sa);
            }
            if (ss->accumulating == 2) ss->accumulating = 1;
        }

        /* We don't want to depend on glWindowPos since it
         * needs OpenGL 1.4, but fortunately the aux context
         * has identity MVP matrix.
         */
        CALL_glPushAttrib(GL_CURRENT_BIT | GL_VIEWPORT_BIT);
        CALL_glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        CALL_glRasterPos2f(-0.9, 0.9);
        for (i = bugle_list_head(&active_samplers); i; i = bugle_list_next(i))
        {
            sa = (sampler *) bugle_list_data(i);
            if (sa->value != HUGE_VAL) /* Skip broken stats */
                render_stats(ss, sa->format, sa->value);
        }
        CALL_glPopAttrib();
        CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
        bugle_end_internal_render("showstats_callback", true);
    }
    return true;
}

static void showstats_accumulate_callback(const xevent_key *key, void *arg, XEvent *event)
{
    showstats_struct *ss;
    ss = bugle_object_get_current_data(&bugle_context_class, showstats_view);
    if (!ss) return;
    /* Value of arg is irrelevant, only truth value */
    ss->accumulating = arg ? 2 : 0;
}

/* Callback to assign the "show" pseudo-variable */
static bool showstats_show(const struct filter_set_variable_info_s *var,
                           const char *text, const void *value)
{
    bugle_list_append(&showstats_requested, bugle_strdup(text));
    return true;
}

static bool initialise_showstats(filter_set *handle)
{
    filter *f;
    bugle_list_node *i, *j;
    sampler *sa;

    f = bugle_register_filter(handle, "showstats");
    bugle_register_filter_depends("invoke", "showstats");
    bugle_register_filter_depends("screenshot", "showstats");
    /* make sure that screenshots capture the stats */
    bugle_register_filter_depends("debugger", "showstats");
    bugle_register_filter_depends("screenshot", "showstats");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, showstats_callback);
    showstats_view = bugle_object_class_register(&bugle_context_class,
                                                 initialise_showstats_struct,
                                                 destroy_showstats_struct,
                                                 sizeof(showstats_struct));

    /* Value of arg is irrelevant, only truth value */
    bugle_register_xevent_key(&key_showstats_accumulate, NULL,
                              showstats_accumulate_callback, f);
    bugle_register_xevent_key(&key_showstats_noaccumulate, NULL,
                              showstats_accumulate_callback, NULL);


    for (i = bugle_list_head(&showstats_requested); i; i = bugle_list_next(i))
    {
        char *name;
        name = (char *) bugle_list_data(i);
        sa = bugle_sampler_find(name);
        if (!sa)
        {
            fprintf(stderr, "Statistic '%s' not found.\nThe registered statistics are:\n", name);
            for (j = bugle_list_head(&registered_samplers); j; j = bugle_list_next(j))
            {
                sa = (sampler *) bugle_list_data(j);
                fprintf(stderr, "  %s\n", sa->name);
            }
            fprintf(stderr, "Note: stats_* should be listed before showstats\n");
            return false;
        }
        else
            bugle_sampler_activate(sa);
    }
    bugle_list_clear(&showstats_requested);

    return true;
}

/*** Loader code ***/

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info showstats_variables[] =
    {
        { "show", "repeat with each item to display", FILTER_SET_VARIABLE_CUSTOM, NULL, showstats_show },
        { "key_accumulate", "frame rate is averaged from time key is pressed [none]", FILTER_SET_VARIABLE_KEY, &key_showstats_accumulate, NULL },
        { "key_noaccumulate", "return frame rate to instantaneous display [none]", FILTER_SET_VARIABLE_KEY, &key_showstats_noaccumulate, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info stats_basic_info =
    {
        "stats_basic",
        initialise_stats_basic,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        "stats module: frames and time"
    };

    static const filter_set_info stats_primitives_info =
    {
        "stats_primitives",
        initialise_stats_primitives,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        "stats module: triangles and batches"
    };

#ifdef GL_ARB_occlusion_query
    static const filter_set_info stats_fragments_info =
    {
        "stats_fragments",
        initialise_stats_fragments,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        "stats module: fragments that pass the depth test"
    };
#endif

#if ENABLE_STATS_NV
    static const filter_set_info stats_nv_info =
    {
        "stats_nv",
        initialise_stats_nv,
        destroy_stats_nv,
        NULL,
        NULL,
        NULL,
        0,
        "stats module: get counters from NVPerfSDK"
    };
#endif /* ENABLE_STATS_NV */

    static const filter_set_info stats_info =
    {
        "stats",
        initialise_stats,
        destroy_stats,
        NULL,
        NULL,
        NULL,
        0,
        NULL
    };

    static const filter_set_info showstats_info =
    {
        "showstats",
        initialise_showstats,
        NULL,
        NULL,
        NULL,
        showstats_variables,
        0,
        "renders information collected by `stats' onto the screen"
    };

    bugle_register_filter_set(&stats_basic_info);
    bugle_register_filter_set_depends("stats_basic", "stats");

    bugle_register_filter_set(&stats_primitives_info);
    bugle_register_filter_set_depends("stats_primitives", "stats");
    bugle_register_filter_set_depends("stats_primitives", "stats_basic");
    bugle_register_filter_set_depends("stats_primitives", "trackcontext");
    bugle_register_filter_set_depends("stats_primitives", "trackdisplaylist");
    bugle_register_filter_set_depends("stats_primitives", "trackbeginend");

#ifdef GL_ARB_occlusion_query
    bugle_register_filter_set(&stats_fragments_info);
    bugle_register_filter_set_depends("stats_fragments", "stats");
    bugle_register_filter_set_depends("stats_fragments", "stats_basic");
    bugle_register_filter_set_depends("stats_fragments", "trackextensions");
    bugle_register_filter_set_renders("stats_fragments");
#endif

#if ENABLE_STATS_NV
    bugle_register_filter_set(&stats_nv_info);
    bugle_register_filter_set_depends("stats_nv", "stats");
    bugle_register_filter_set_depends("stats_nv", "stats_basic");
#endif

    bugle_register_filter_set(&stats_info);
    bugle_register_filter_set(&showstats_info);
    bugle_register_filter_set_depends("showstats", "stats");
    bugle_register_filter_set_renders("showstats");
    bugle_list_init(&showstats_requested, true);
}
