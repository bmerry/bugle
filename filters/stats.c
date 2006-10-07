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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <math.h>
#include "src/glutils.h"
#include "src/tracker.h"
#include "src/filters.h"
#include "src/log.h"
#include "src/xevent.h"
#include "src/glexts.h"
#include "filters/stats.h"
#include "filters/statsparse.h"
#include "common/safemem.h"
#include "common/hashtable.h"
#include "common/bool.h"

#define STATISTICSFILE "/.bugle/statistics"

#if HAVE_FINITE
# define FINITE(x) (finite(x))
#elif HAVE_ISFINITE
# define FINITE(x) (isfinite(x))
#else
# define FINITE(x) ((x) != (x) && (x) != HUGE_VAL && (x) != -HUGE_VAL)
#endif

#if HAVE_ISNAN
# define ISNAN(x) isnan(x)
#else
# define ISNAN(x) ((x) != (x))
#endif

#if HAVE_NAN
# define NAN() (nan(""))
#else
# define NAN() (0.0 / 0.0)
#endif

extern FILE *yyin;
extern int stats_yyparse(void);

typedef struct
{
    double value;
    double integral;
} stats_signal_value;

typedef struct
{
    int allocated;
    stats_signal_value *values;
    struct timeval last_updated;
} stats_signal_values;

static bugle_hash_table stats_signals;
static size_t stats_signals_num_active = 0;
static bugle_linked_list stats_signals_active;
static bugle_linked_list *stats_statistics = NULL;
static bugle_hash_table stats_statistics_table;

/*** Low-level utilities ***/

static double time_elapsed(struct timeval *old, struct timeval *now)
{
    return (now->tv_sec - old->tv_sec) + 1e-6 * (now->tv_usec - old->tv_usec);
}

/* If the signal does not exist, it is created. Newly created signals do
 * not have a slot number (slot -1); only activated signals get slots.
 * This allows the configuration file to specify non-existant signals.
 * FIXME: we really should warn when this happens, since otherwise stats
 * will be silently omitted if the generator is missing from the chain.
 */
static stats_signal *stats_signal_get(const char *name)
{
    stats_signal *si;

    si = bugle_hash_get(&stats_signals, name);
    if (!si)
    {
        si = (stats_signal *) bugle_malloc(sizeof(stats_signal));
        si->value = NAN();
        si->integral = 0.0;
        si->last_updated.tv_sec = 0;
        si->last_updated.tv_usec = 0;
        si->active = false;
        si->offset = -1;
        si->user_data = NULL;
        si->activate = NULL;
        bugle_hash_set(&stats_signals, name, si);
    }
    return si;
}

static stats_signal *stats_signal_register(const char *name, void *user_data,
                                           bool (*activate)(stats_signal *))
{
    stats_signal *si;

    si = stats_signal_get(name);
    si->user_data = user_data;
    si->activate = activate;
    return si;
}

/* Evaluates the expression. If a signal is missing, the return is NaN. */
static double stats_expression_evaluate(const stats_expression *expr,
                                        stats_signal_values *old_signals,
                                        stats_signal_values *new_signals)
{
    double l = 0.0, r = 0.0;
    double elapsed;

    elapsed = time_elapsed(&old_signals->last_updated, &new_signals->last_updated);
    switch (expr->type)
    {
    case STATS_EXPRESSION_NUMBER:
        return expr->value;
    case STATS_EXPRESSION_OPERATION:
        if (expr->left)
            l = stats_expression_evaluate(expr->left, old_signals, new_signals);
        if (expr->right)
            r = stats_expression_evaluate(expr->right, old_signals, new_signals);
        switch (expr->op)
        {
        case STATS_OPERATION_PLUS: return l + r;
        case STATS_OPERATION_MINUS: return l - r;
        case STATS_OPERATION_TIMES: return l * r;
        case STATS_OPERATION_DIVIDE: return l / r;
        case STATS_OPERATION_UMINUS: return -l;
        default: abort(); /* Should never be reached */
        }
    case STATS_EXPRESSION_SIGNAL:
        /* Generate a NaN, and let good old IEEE 754 take care of propagating it. */
        if (!expr->signal->active) return NAN();
        switch (expr->op)
        {
        case STATS_OPERATION_DELTA:
            return new_signals->values[expr->signal->offset].value - old_signals->values[expr->signal->offset].value;
        case STATS_OPERATION_AVERAGE:
            return (new_signals->values[expr->signal->offset].integral - old_signals->values[expr->signal->offset].integral) / elapsed;
        case STATS_OPERATION_START:
            return old_signals->values[expr->signal->offset].value;
        case STATS_OPERATION_END:
            return new_signals->values[expr->signal->offset].value;
        default:
            abort(); /* Should never be reached */
        }
    }
    abort(); /* Should never be reached */
}

static void stats_signal_update(stats_signal *si, double v)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    /* Integrate over time; a NaN indicates that this is the first time */
    if (FINITE(si->value))
        si->integral += time_elapsed(&si->last_updated, &now) * si->value;
    si->value = v;
    si->last_updated = now;
}

/* Convenience for accumulating signals */
static void stats_signal_add(stats_signal *si, double dv)
{
    if (!FINITE(si->value))
        stats_signal_update(si, dv);
    else
        stats_signal_update(si, si->value + dv);
}

static void stats_signal_activate(stats_signal *si)
{
    if (!si->active)
    {
        si->active = true;
        if (si->activate)
            si->active = (*si->activate)(si);
        if (si->active)
        {
            si->offset = stats_signals_num_active++;
            bugle_list_append(&stats_signals_active, si);
        }
    }
}

static void stats_signal_values_init(stats_signal_values *sv)
{
    sv->allocated = 0;
    sv->values = NULL;
    sv->last_updated.tv_sec = 0;
    sv->last_updated.tv_usec = 0;
}

static void stats_signal_values_clear(stats_signal_values *sv)
{
    free(sv->values);
    sv->values = NULL;
    sv->allocated = 0;
    sv->last_updated.tv_sec = 0;
    sv->last_updated.tv_usec = 0;
}

static void stats_signal_values_gather(stats_signal_values *sv)
{
    stats_signal *si;
    bugle_list_node *s;
    int i;

    gettimeofday(&sv->last_updated, NULL);

    if (sv->allocated < stats_signals_num_active)
    {
        sv->allocated = stats_signals_num_active;
        sv->values = bugle_realloc(sv->values, stats_signals_num_active * sizeof(stats_signal_value));
    }
    /* Make sure that everything is initialised to an insane value (NaN) */
    for (i = 0; i < stats_signals_num_active; i++)
        sv->values[i].value = sv->values[i].integral = 0.0 / 0.0;
    for (s = bugle_list_head(&stats_signals_active); s; s = bugle_list_next(s))
    {
        si = (stats_signal *) bugle_list_data(s);
        if (si->active && si->offset >= 0)
        {
            sv->values[si->offset].value = si->value;
            sv->values[si->offset].integral = si->integral;
            /* Have to update the integral from when the signal was updated
             * until this instant. */
            if (FINITE(si->value))
                sv->values[si->offset].integral +=
                    si->value * time_elapsed(&si->last_updated,
                                             &sv->last_updated);
        }
    }
}

/* Fills in the signal field of signal expressions */
static void stats_expression_initialise_signals(stats_expression *expr)
{
    switch (expr->type)
    {
    case STATS_EXPRESSION_NUMBER: break;
    case STATS_EXPRESSION_OPERATION:
        if (expr->left) stats_expression_initialise_signals(expr->left);
        if (expr->right) stats_expression_initialise_signals(expr->right);
        break;
    case STATS_EXPRESSION_SIGNAL:
        expr->signal = stats_signal_get(expr->signal_name);
        break;
    }
}

/* Activates all signals that are this expression depends on */
static void stats_expression_activate_signals(stats_expression *expr)
{
    switch (expr->type)
    {
    case STATS_EXPRESSION_NUMBER: break;
    case STATS_EXPRESSION_OPERATION:
        if (expr->left) stats_expression_activate_signals(expr->left);
        if (expr->right) stats_expression_activate_signals(expr->right);
        break;
    case STATS_EXPRESSION_SIGNAL:
        stats_signal_activate(expr->signal);
        break;
    }
}

/* Frees the expression object itself and its children */
static void stats_expression_free(stats_expression *expr)
{
    assert(expr);
    switch (expr->type)
    {
    case STATS_EXPRESSION_NUMBER: break;
    case STATS_EXPRESSION_OPERATION:
        if (expr->left) stats_expression_free(expr->left);
        if (expr->right) stats_expression_free(expr->right);
        break;
    case STATS_EXPRESSION_SIGNAL:
        free(expr->signal_name);
        break;
    }
    free(expr);
}

static stats_substitution *stats_statistic_find_substitution(stats_statistic *st, double v)
{
    bugle_list_node *i;
    stats_substitution *cur;

    for (i = bugle_list_head(&st->substitutions); i; i = bugle_list_next(i))
    {
        cur = (stats_substitution *) bugle_list_data(i);
        if (fabs(cur->value - v) < 1e-9) return cur;
    }
    return NULL;
}

static void stats_statistic_free(stats_statistic *st)
{
    bugle_list_node *i;
    stats_substitution *sub;

    free(st->name);
    stats_expression_free(st->value);
    free(st->label);
    for (i = bugle_list_head(&st->substitutions); i; i = bugle_list_next(i))
    {
        sub = (stats_substitution *) bugle_list_data(i);
        free(sub->replacement);
    }
    bugle_list_clear(&st->substitutions);
    free(st);
}

/* Returns the statistic if it is registered, or NULL if not. Note that
 * it is returned even if some signals are missing; call
 * stats_statistic_complete to determine whether this is the case.
 * FIXME: that function does not yet exist.
 */
static stats_statistic *stats_statistic_find(const char *name)
{
    return (stats_statistic *) bugle_hash_get(&stats_statistics_table, name);
}

/* List the registered statistics, for when an illegal one is mentioned */
static void stats_statistic_list(const char *caller)
{
    bugle_list_node *j;
    stats_statistic *st;

    fputs("The registered statistics are:\n", stderr);

    for (j = bugle_list_head(stats_statistics); j; j = bugle_list_next(j))
    {
        st = (stats_statistic *) bugle_list_data(j);
        fprintf(stderr, "  %s\n", st->name);
    }
    if (caller)
        fprintf(stderr, "Note: statistics generators should be listed before %s\n", caller);
}

/*** stats filterset ***/

/* Reads the statistics configuration file. On error, prints a message
 * to stderr and returns false.
 */
static bool stats_load_config(void)
{
    const char *home;
    char *config = NULL;
    bugle_list_node *i;
    stats_statistic *st;

    if (getenv("BUGLE_STATISTICS"))
        config = bugle_strdup(getenv("BUGLE_STATISTICS"));
    home = getenv("HOME");
    /* If using the debugger and no chain is specified, we use passthrough
     * mode.
     */
    if (!config && !home)
    {
        fputs("Please set BUGLE_STATISTICS\n", stderr);
        return false;
    }
    if (!config)
    {
        config = bugle_malloc(strlen(home) + strlen(STATISTICSFILE) + 1);
        sprintf(config, "%s%s", home, STATISTICSFILE);
    }
    if ((yyin = fopen(config, "r")) != NULL)
    {
        if (stats_yyparse() == 0)
        {
            stats_statistics = stats_statistics_get_list();
            for (i = bugle_list_head(stats_statistics); i; i = bugle_list_next(i))
            {
                st = (stats_statistic *) bugle_list_data(i);
                stats_expression_initialise_signals(st->value);
                bugle_hash_set(&stats_statistics_table, st->name, st);
            }
            free(config);
            return true;
        }
        else
        {
            fprintf(stderr, "Parse error in %s\n", config);
            free(config);
            return false;
        }
    }
    else
    {
        fprintf(stderr, "Failed to open %s\n", config);
        free(config);
        return false;
    }
}

static bool stats_initialise(filter_set *handle)
{
    bugle_hash_init(&stats_signals, true);
    bugle_list_init(&stats_signals_active, false);
    bugle_hash_init(&stats_statistics_table, false);
    if (!stats_load_config()) return false;

    /* This filter does no interception, but it provides a sequence point
     * to allow display modules (showstats, logstats) to occur after
     * statistics modules (stats_basic etc).
     */
    bugle_register_filter(handle, "stats");
    return true;
}

static void stats_destroy(filter_set *handle)
{
    bugle_list_node *i;
    stats_statistic *st;

    if (stats_statistics)
    {
        for (i = bugle_list_head(stats_statistics); i; i = bugle_list_next(i))
        {
            st = (stats_statistic *) bugle_list_data(i);
            stats_statistic_free(st);
        }
        bugle_list_clear(stats_statistics);
    }
    bugle_list_clear(&stats_signals_active);
    bugle_hash_clear(&stats_signals);
    bugle_hash_clear(&stats_statistics_table);
}

/*** Generators ***/

static stats_signal *stats_basic_frames, *stats_basic_seconds;

static bool stats_basic_seconds_activate(stats_signal *si)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    stats_signal_update(si, t.tv_sec + 1e-6 * t.tv_usec);
    return true;
}

static bool stats_basic_glXSwapBuffers(function_call *call, const callback_data *data)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    stats_signal_update(stats_basic_seconds, now.tv_sec + 1e-6 * now.tv_usec);
    stats_signal_add(stats_basic_frames, 1.0);
    return true;
}

static bool stats_basic_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "stats_basic");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, stats_basic_glXSwapBuffers);
    bugle_register_filter_depends("invoke", "stats_basic");
    bugle_register_filter_depends("stats", "stats_basic");

    stats_basic_frames = stats_signal_register("frames", NULL, NULL);
    stats_basic_seconds = stats_signal_register("seconds", NULL, stats_basic_seconds_activate);
    return true;
}


static bugle_object_view stats_primitives_view;  /* begin/end counting */
static bugle_object_view stats_primitives_displaylist_view;
static stats_signal *stats_primitives_batches, *stats_primitives_triangles;

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

/* Increments the triangle and batch count according to mode */
static void stats_primitives_update(GLenum mode, GLsizei count)
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

    switch (bugle_displaylist_mode())
    {
    case GL_NONE:
        stats_signal_add(stats_primitives_triangles, t);
        stats_signal_add(stats_primitives_batches, 1);
        break;
    case GL_COMPILE_AND_EXECUTE:
        stats_signal_add(stats_primitives_triangles, t);
        stats_signal_add(stats_primitives_batches, 1);
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
    stats_primitives_update(s->begin_mode, s->begin_count);
    s->begin_mode = GL_NONE;
    s->begin_count = 0;
    return true;
}

static bool stats_primitives_glDrawArrays(function_call *call, const callback_data *data)
{
    stats_primitives_update(*call->typed.glDrawArrays.arg0, *call->typed.glDrawArrays.arg2);
    return true;
}

static bool stats_primitives_glDrawElements(function_call *call, const callback_data *data)
{
    stats_primitives_update(*call->typed.glDrawElements.arg0, *call->typed.glDrawElements.arg1);
    return true;
}

#ifdef GL_EXT_draw_range_elements
static bool stats_primitives_glDrawRangeElements(function_call *call, const callback_data *data)
{
    stats_primitives_update(*call->typed.glDrawRangeElementsEXT.arg0, *call->typed.glDrawRangeElementsEXT.arg3);
    return true;
}
#endif

#ifdef GL_EXT_multi_draw_arrays
static bool stats_primitives_glMultiDrawArrays(function_call *call, const callback_data *data)
{
    GLsizei i, primcount;

    primcount = *call->typed.glMultiDrawArrays.arg3;
    for (i = 0; i < primcount; i++)
        stats_primitives_update(*call->typed.glMultiDrawArrays.arg0,
                                (*call->typed.glMultiDrawArrays.arg2)[i]);
    return true;
}

static bool stats_primitives_glMultiDrawElements(function_call *call, const callback_data *data)
{
    GLsizei i, primcount;

    primcount = *call->typed.glMultiDrawElements.arg4;
    for (i = 0; i < primcount; i++)
        stats_primitives_update(*call->typed.glMultiDrawElements.arg0,
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
        stats_signal_add(stats_primitives_triangles, counts->triangles);
        stats_signal_add(stats_primitives_batches, counts->batches);
    }
    return true;
}

static bool stats_primitives_glCallLists(function_call *call, const callback_data *data)
{
    fputs("FIXME: triangle counting in glCallLists not implemented!\n", stderr);
    return true;
}

static bool stats_primitives_initialise(filter_set *handle)
{
    filter *f;

    stats_primitives_view = bugle_object_class_register(&bugle_context_class,
                                                        NULL,
                                                        NULL,
                                                        sizeof(stats_primitives_struct));
    stats_primitives_displaylist_view = bugle_object_class_register(&bugle_displaylist_class,
                                                                    NULL,
                                                                    NULL,
                                                                    sizeof(stats_primitives_struct));

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

    stats_primitives_batches = stats_signal_register("batches", NULL, NULL);
    stats_primitives_triangles = stats_signal_register("triangles", NULL, NULL);

    return true;
}


#ifdef GL_ARB_occlusion_query

typedef struct
{
    GLuint query;
} stats_fragments_struct;

static stats_signal *stats_fragments_fragments;
static bugle_object_view stats_fragments_view;

static void stats_fragments_struct_initialise(const void *key, void *data)
{
    stats_fragments_struct *s;

    s = (stats_fragments_struct *) data;
    if (stats_fragments_fragments->active
        && bugle_gl_has_extension(BUGLE_GL_ARB_occlusion_query)
        && bugle_begin_internal_render())
    {
        CALL_glGenQueriesARB(1, &s->query);
        if (s->query)
            CALL_glBeginQueryARB(GL_SAMPLES_PASSED_ARB, s->query);
        bugle_end_internal_render("stats_fragments_struct_initialise", true);
    }
}

static void stats_fragments_struct_destroy(void *data)
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
    if (stats_fragments_fragments->active
        && s && s->query && bugle_begin_internal_render())
    {
        CALL_glEndQueryARB(GL_SAMPLES_PASSED_ARB);
        CALL_glGetQueryObjectuivARB(s->query, GL_QUERY_RESULT_ARB, &fragments);
        bugle_end_internal_render("stats_fragments_glXSwapBuffers", true);
        stats_signal_add(stats_fragments_fragments, fragments);
    }
    return true;
}

static bool stats_fragments_post_glXSwapBuffers(function_call *call, const callback_data *data)
{
    stats_fragments_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_fragments_view);
    if (stats_fragments_fragments->active
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
    if (stats_fragments_fragments->active
        && s->query)
    {
        fputs("App is using occlusion queries, disabling fragment counting\n", stderr);
        CALL_glEndQueryARB(GL_SAMPLES_PASSED_ARB);
        CALL_glDeleteQueriesARB(1, &s->query);
        s->query = 0;
        stats_fragments_fragments->active = false;
    }
    return true;
}

static bool stats_fragments_initialise(filter_set *handle)
{
    filter *f;

    stats_fragments_view = bugle_object_class_register(&bugle_context_class,
                                                       stats_fragments_struct_initialise,
                                                       stats_fragments_struct_destroy,
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

    stats_fragments_fragments = stats_signal_register("fragments", NULL, NULL);
    return true;
}
#endif /* GL_ARB_occlusion_query */


#if HAVE_NVPERFSDK_H
#include <NVPerfSDK.h>
#include <ltdl.h>

typedef struct
{
    UINT index;
    bool use_cycles;
    bool accumulate;
    bool experiment;
} stats_signal_nv;

static bugle_linked_list stats_nv_active;
static bugle_linked_list stats_nv_registered;
static lt_dlhandle stats_nv_dl = NULL;
static bool stats_nv_experiment_mode = false; /* True if using simplified experiments */
static int stats_nv_num_passes = -1, stats_nv_pass = -1;
static bool stats_nv_flush = false;

static NVPMRESULT (*fNVPMInit)(void);
static NVPMRESULT (*fNVPMShutdown)(void);
static NVPMRESULT (*fNVPMEnumCounters)(NVPMEnumFunc);
static NVPMRESULT (*fNVPMSample)(NVPMSampleValue *, UINT *);
static NVPMRESULT (*fNVPMBeginExperiment)(int *);
static NVPMRESULT (*fNVPMEndExperiment)(void);
static NVPMRESULT (*fNVPMBeginPass)(int);
static NVPMRESULT (*fNVPMEndPass)(int);
static NVPMRESULT (*fNVPMAddCounter)(UINT);
static NVPMRESULT (*fNVPMRemoveAllCounters)(void);
static NVPMRESULT (*fNVPMGetCounterValue)(UINT, int, UINT64 *, UINT64 *);
static NVPMRESULT (*fNVPMGetCounterAttribute)(UINT, NVPMATTRIBUTE, UINT64 *);

static NVPMRESULT check_nvpm(NVPMRESULT status, const char *file, int line)
{
    if (status != NVPM_OK)
    {
        fprintf(stderr, "%s:%d: NVPM error %d\n", file, line, (int) status);
        exit(1);
    }
    return status;
}

#define CHECK_NVPM(x) (check_nvpm((x), __FILE__, __LINE__))

static bool stats_nv_glXSwapBuffers(function_call *call, const callback_data *data)
{
    bugle_list_node *i;
    stats_signal *si;
    stats_signal_nv *nv;
    UINT samples;
    UINT64 value, cycles;
    bool experiment_done = false;

    if (bugle_list_head(&stats_nv_active))
    {
        if (stats_nv_experiment_mode)
        {
            if (stats_nv_pass >= 0)
            {
                CHECK_NVPM(fNVPMEndPass(stats_nv_pass));
                if (stats_nv_flush) CALL_glFinish();
                if (stats_nv_pass + 1 == stats_nv_num_passes)
                {
                    CHECK_NVPM(fNVPMEndExperiment());
                    experiment_done = true;
                    stats_nv_pass = -1;    /* tag as not-in-experiment */
                }
            }
        }

        if (stats_nv_flush) CALL_glFinish();
        CHECK_NVPM(fNVPMSample(NULL, &samples));
        for (i = bugle_list_head(&stats_nv_active); i; i = bugle_list_next(i))
        {
            si = (stats_signal *) bugle_list_data(i);
            nv = (stats_signal_nv *) si->user_data;
            if (nv->experiment && !experiment_done) continue;

            CHECK_NVPM(fNVPMGetCounterValue(nv->index, 0, &value, &cycles));
            if (nv->use_cycles) value = cycles;
            if (nv->accumulate) stats_signal_add(si, value);
            else stats_signal_update(si, value);
        }
    }
    return true;
}

static bool stats_nv_post_glXSwapBuffers(function_call *call, const callback_data *data)
{
    if (stats_nv_experiment_mode)
    {
        stats_nv_pass++;
        if (stats_nv_pass == 0)
            CHECK_NVPM(fNVPMBeginExperiment(&stats_nv_num_passes));
        CHECK_NVPM(fNVPMBeginPass(stats_nv_pass));
    }
    return true;
}

static bool stats_nv_signal_activate(stats_signal *st)
{
    stats_signal_nv *nv;

    nv = (stats_signal_nv *) st->user_data;
    if (fNVPMAddCounter(nv->index) != NVPM_OK) return false;
    if (nv->experiment) stats_nv_experiment_mode = true;
    bugle_list_append(&stats_nv_active, st);
    return true;
}

static int stats_nv_enumerate(UINT index, char *name)
{
    char *stat_name;
    stats_signal *si;
    stats_signal_nv *nv;
    UINT64 counter_type;
    int accum, cycles;

    fNVPMGetCounterAttribute(index, NVPMA_COUNTER_TYPE, &counter_type);
    for (accum = 0; accum < 2; accum++)
        for (cycles = 0; cycles < 2; cycles++)
        {
            bugle_asprintf(&stat_name, "nv%s%s:%s",
                           accum ? ":accum" : "",
                           cycles ? ":cycles" : "", name);
            nv = bugle_malloc(sizeof(stats_signal_nv));
            nv->index = index;
            nv->accumulate = (accum == 1);
            nv->use_cycles = (cycles == 1);
            nv->experiment = (counter_type == NVPM_CT_SIMEXP);
            si = stats_signal_register(stat_name, nv,
                                       stats_nv_signal_activate);
            bugle_list_append(&stats_nv_registered, si);
            free(stat_name);
        }
    return NVPM_OK;
}

static bool stats_nv_initialise(filter_set *handle)
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
    fNVPMBeginExperiment = (NVPMRESULT (*)(int *)) lt_dlsym(stats_nv_dl, "NVPMBeginExperiment");
    fNVPMEndExperiment = (NVPMRESULT (*)(void)) lt_dlsym(stats_nv_dl, "NVPMEndExperiment");
    fNVPMBeginPass = (NVPMRESULT (*)(int)) lt_dlsym(stats_nv_dl, "NVPMBeginPass");
    fNVPMEndPass = (NVPMRESULT (*)(int)) lt_dlsym(stats_nv_dl, "NVPMEndPass");
    fNVPMRemoveAllCounters = (NVPMRESULT (*)(void)) lt_dlsym(stats_nv_dl, "NVPMRemoveAllCounters");
    fNVPMAddCounter = (NVPMRESULT (*)(UINT)) lt_dlsym(stats_nv_dl, "NVPMAddCounter");
    fNVPMGetCounterValue = (NVPMRESULT (*)(UINT, int, UINT64 *, UINT64 *)) lt_dlsym(stats_nv_dl, "NVPMGetCounterValue");
    fNVPMGetCounterAttribute = (NVPMRESULT (*)(UINT, NVPMATTRIBUTE, UINT64 *)) lt_dlsym(stats_nv_dl, "NVPMGetCounterAttribute");

    if (!fNVPMInit
        || !fNVPMShutdown
        || !fNVPMEnumCounters
        || !fNVPMSample
        || !fNVPMBeginExperiment || !fNVPMEndExperiment
        || !fNVPMBeginPass || !fNVPMEndPass
        || !fNVPMAddCounter || !fNVPMRemoveAllCounters
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

    f = bugle_register_filter(handle, "stats_nv_post");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, stats_nv_post_glXSwapBuffers);
    bugle_register_filter_depends("stats_nv_post", "invoke");
    return true;

cancel2:
    fNVPMShutdown();
cancel1:
    lt_dlclose(stats_nv_dl);
    stats_nv_dl = NULL;
    return false;
}

static void stats_nv_destroy(filter_set *handle)
{
    bugle_list_node *i;
    stats_signal *si;

    for (i = bugle_list_head(&stats_nv_registered); i; i = bugle_list_next(i))
    {
        si = (stats_signal *) bugle_list_data(i);
        free(si->user_data);
    }
    fNVPMRemoveAllCounters();
    fNVPMShutdown();
    lt_dlclose(stats_nv_dl);
    bugle_list_clear(&stats_nv_registered);
    bugle_list_clear(&stats_nv_active);
}

#endif /* HAVE_NVPERFSDK_H */

/*** Printers ***/

static bugle_linked_list logstats_show;    /* actual stats */
static bugle_linked_list logstats_show_requested;  /* names in config file */
static stats_signal_values logstats_prev, logstats_cur;

/* Callback to assign the "show" pseudo-variable */
static bool logstats_show_set(const struct filter_set_variable_info_s *var,
                              const char *text, const void *value)
{
    bugle_list_append(&logstats_show_requested, bugle_strdup(text));
    return true;
}

static bool logstats_glXSwapBuffers(function_call *call, const callback_data *data)
{
    char *msg;
    bugle_list_node *i;
    stats_statistic *st;
    stats_signal_values tmp;
    stats_substitution *sub;

    tmp = logstats_prev;
    logstats_prev = logstats_cur;
    logstats_cur = tmp;
    stats_signal_values_gather(&logstats_cur);
    if (logstats_prev.allocated)
    {
        for (i = bugle_list_head(&logstats_show); i; i = bugle_list_next(i))
        {
            st = (stats_statistic *) bugle_list_data(i);
            double v = stats_expression_evaluate(st->value, &logstats_prev, &logstats_cur);
            if (FINITE(v))
            {
                sub = stats_statistic_find_substitution(st, v);
                if (sub)
                    bugle_asprintf(&msg, "%s %s", sub->replacement,
                                   st->label ? st->label : "");
                else
                    bugle_asprintf(&msg, "%.*f %s", st->precision, v,
                                   st->label ? st->label : "");
                bugle_log("logstats", st->name, msg);
            }
        }
    }
    return true;
}

static bool logstats_initialise(filter_set *handle)
{
    filter *f;
    bugle_list_node *i;
    stats_statistic *st;

    f = bugle_register_filter(handle, "stats_log");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, logstats_glXSwapBuffers);
    bugle_log_register_filter("stats_log");

    bugle_list_clear(&logstats_show);
    for (i = bugle_list_head(&logstats_show_requested); i; i = bugle_list_next(i))
    {
        char *name;
        name = (char *) bugle_list_data(i);
        st = stats_statistic_find(name);
        if (!st)
        {
            fprintf(stderr, "Statistic '%s' not found.\n", name);
            stats_statistic_list("logstats");
            return false;
        }
        else
        {
            stats_expression_activate_signals(st->value);
            bugle_list_append(&logstats_show, st);
        }
    }
    bugle_list_clear(&logstats_show_requested);
    stats_signal_values_init(&logstats_prev);
    stats_signal_values_init(&logstats_cur);
    return true;
}

static void logstats_destroy(filter_set *handle)
{
    stats_signal_values_clear(&logstats_prev);
    stats_signal_values_clear(&logstats_cur);
    bugle_list_clear(&logstats_show);
}


typedef struct
{
    GLuint font_base;
    struct timeval last_update;
    int accumulating;  /* 0: no  1: yes  2: yes, reset counters */
} showstats_struct;

typedef enum
{
    SHOWSTATS_TEXT,
    SHOWSTATS_GRAPH
} showstats_mode;

typedef struct
{
    char *name;
    showstats_mode mode;
    stats_statistic *st;
    bool initialised;

    /* Graph-specific stuff */
    double graph_scale;     /* Largest value on graph */
    GLsizei graph_size;     /* Number of history samples */
    double *graph_history;  /* Raw (unscaled) history values */
    GLushort *graph_scaled; /* Scaled according to graph_scale for OpenGL */
    int graph_offset;       /* place to put next sample */

    GLuint graph_tex;       /* 1D shadow texture to determine graph height */
} showstats_statistic;

static bugle_object_view showstats_view;
static bugle_linked_list showstats_stats;  /* List of showstats_statistic */
static xevent_key key_showstats_accumulate = { NoSymbol, 0, true };
static xevent_key key_showstats_noaccumulate = { NoSymbol, 0, true };

static stats_signal_values showstats_prev, showstats_cur;
static char *showstats_display = NULL;
static size_t showstats_display_size = 0;

/* Renders a string of text to screen. The raster position is destroyed */
static void showstats_render(showstats_struct *ss, const char *msg)
{
    Display *dpy;
    Font f;
    const char *ch;

    if (!ss->font_base)
    {
        dpy = CALL_glXGetCurrentDisplay();
        ss->font_base = CALL_glGenLists(256);
        f = XLoadFont(dpy, "-*-courier-medium-r-normal--10-*");
        CALL_glXUseXFont(f, 0, 256, ss->font_base);
        XUnloadFont(dpy, f);
    }

    CALL_glPushAttrib(GL_CURRENT_BIT); /* Save start of line pos */
    for (ch = msg; *ch; ch++)
    {
        if (*ch == '\n')
        {
            CALL_glPopAttrib(); /* Move back to start of line */
            CALL_glBitmap(0, 0, 0, 0, 0, -16, NULL); /* Move to next line */
            CALL_glPushAttrib(GL_CURRENT_BIT);  /* Re-save start of line pos */
        }
        else
            CALL_glCallList(ss->font_base + *ch);
    }
    CALL_glPopAttrib();
}

static void showstats_struct_initialise(const void *key, void *data)
{
    showstats_struct *ss;

    /* We want to create the display lists in the unshared context, so
     * we defer until the first call to showstats_render.
     */
    ss = (showstats_struct *) data;
    ss->font_base = 0;
    ss->last_update.tv_sec = 0;
    ss->last_update.tv_usec = 0;
    ss->accumulating = 0;
}

static void showstats_struct_destroy(void *data)
{
    showstats_struct *ss;
    ss = (showstats_struct *) data;
    CALL_glDeleteLists(1, ss->font_base);
}

/* Creates the extended data (e.g. OpenGL objects) that depend on having
 * the aux context active. FIXME: cleanup of this state.
 */
static void showstats_statistic_initialise(showstats_statistic *sst)
{
    if (sst->initialised) return;
    switch (sst->mode)
    {
    case SHOWSTATS_TEXT:
        sst->initialised = true;
        break;
    case SHOWSTATS_GRAPH:
#if defined(GL_ARB_shadow) && defined(GL_ARB_depth_texture)
        if (!bugle_gl_has_extension(BUGLE_GL_ARB_shadow)
            || !bugle_gl_has_extension(BUGLE_GL_ARB_depth_texture))
#endif
        {
            fputs("Graphing currently required shadow map support. Fallback code\n"
                  "is on the TODO list.\n", stderr);
            exit(1);
        }
#if defined(GL_ARB_shadow) && defined(GL_ARB_depth_texture)
        else if (bugle_begin_internal_render())
        {
            GLint max_size;

            sst->graph_size = 128;
            CALL_glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
            if (max_size < sst->graph_size)
                sst->graph_size = max_size;
            sst->graph_history = (double *) bugle_calloc(sst->graph_size, sizeof(double));
            sst->graph_scaled = (GLushort *) bugle_calloc(sst->graph_size, sizeof(GLushort));
            sst->graph_scale = sst->st->maximum;

            CALL_glGenTextures(1, &sst->graph_tex);
            CALL_glBindTexture(GL_TEXTURE_1D, sst->graph_tex);
            CALL_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            CALL_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            CALL_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            CALL_glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
            CALL_glTexImage1D(GL_TEXTURE_1D, 0, GL_DEPTH_COMPONENT16_ARB,
                              sst->graph_size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, sst->graph_scaled);
            CALL_glBindTexture(GL_TEXTURE_1D, 0);
            bugle_end_internal_render("showstats_statistic_initialise", true);
            sst->initialised = true;
        }
#endif
        break;
    }
}

static void showstats_graph_rescale(showstats_statistic *sst, double new_scale)
{
    double p10 = 1.0;
    double s = 0.0, v;
    int i;
    /* Find a suitably large, "nice" value */

    while (new_scale > 5.0)
    {
        p10 *= 10.0;
        new_scale /= 10.0;
    }
    if (new_scale <= 1.0) s = p10;
    else if (new_scale <= 2.0) s = 2.0 * p10;
    else s = 5.0 * p10;

    sst->graph_scale = s;
    /* Regenerate the texture at the new scale */
    for (i = 0; i < sst->graph_size; i++)
    {
        v = sst->graph_history[i] >= 0.0 ? sst->graph_history[i] : 0.0;
        sst->graph_scaled[i] = (GLushort) rint(v * 65535.0 / s);
    }
    CALL_glBindTexture(GL_TEXTURE_1D, sst->graph_tex);
    CALL_glTexSubImage1D(GL_TEXTURE_1D, 0, 0, sst->graph_size,
                         GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, sst->graph_scaled);
    CALL_glBindTexture(GL_TEXTURE_1D, 0);
}

static bool showstats_glXSwapBuffers(function_call *call, const callback_data *data)
{
    Display *dpy;
    GLXDrawable old_read, old_write;
    GLXContext aux, real;
    showstats_struct *ss;
    GLint viewport[4];
    bugle_list_node *i;
    showstats_statistic *sst;
    stats_substitution *sub;
    stats_signal_values tmp;
    double v;
    struct timeval now;

    ss = bugle_object_get_current_data(&bugle_context_class, showstats_view);
    gettimeofday(&now, NULL);
    aux = bugle_get_aux_context(false);
    if (aux && bugle_begin_internal_render())
    {
        CALL_glGetIntegerv(GL_VIEWPORT, viewport);
        real = CALL_glXGetCurrentContext();
        old_write = CALL_glXGetCurrentDrawable();
        old_read = CALL_glXGetCurrentReadDrawable();
        dpy = CALL_glXGetCurrentDisplay();
        CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);

        /* FIXME: make the time interval tunable */
        if (time_elapsed(&ss->last_update, &now) >= 0.2)
        {
            ss->last_update = now;
            stats_signal_values_gather(&showstats_cur);

            if (showstats_prev.allocated)
            {
                if (showstats_display) showstats_display[0] = '\0';
                for (i = bugle_list_head(&showstats_stats); i; i = bugle_list_next(i))
                {
                    sst = (showstats_statistic *) bugle_list_data(i);
                    if (!sst->initialised) showstats_statistic_initialise(sst);
                    v = stats_expression_evaluate(sst->st->value, &showstats_prev, &showstats_cur);
                    switch (sst->mode)
                    {
                    case SHOWSTATS_TEXT:
                        if (FINITE(v))
                        {
                            sub = stats_statistic_find_substitution(sst->st, v);
                            if (sub)
                                bugle_appendf(&showstats_display, &showstats_display_size,
                                              "%10s %s\n", sub->replacement, sst->st->label);
                            else
                                bugle_appendf(&showstats_display, &showstats_display_size,
                                              "%10.*f %s\n", sst->st->precision, v, sst->st->label);
                        }
                        break;
                    case SHOWSTATS_GRAPH:
                        if (sst->graph_tex)
                        {
                            GLushort vs;

                            if (!FINITE(v)) v = 0.0;
                            sst->graph_history[sst->graph_offset] = v;
                            /* Check if we need to rescale */
                            if (v > sst->graph_scale)
                                showstats_graph_rescale(sst, v);
                            v /= sst->graph_scale;
                            if (v < 0.0) v = 0.0;
                            vs = (GLushort) rint(v * 65535.0);
                            CALL_glBindTexture(GL_TEXTURE_1D, sst->graph_tex);
                            CALL_glTexSubImage1D(GL_TEXTURE_1D, 0, sst->graph_offset, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, &vs);
                            CALL_glBindTexture(GL_TEXTURE_1D, 0);

                            sst->graph_scaled[sst->graph_offset] = vs;
                            sst->graph_offset++;
                            if (sst->graph_offset >= sst->graph_size)
                                sst->graph_offset = 0;
                        }
                        break;
                    }
                }
            }
            if (ss->accumulating != 1 || !showstats_prev.allocated)
            {
                /* Bring prev up to date for next time. Swap so that we recycle
                 * memory for next time.
                 */
                tmp = showstats_prev;
                showstats_prev = showstats_cur;
                showstats_cur = tmp;
            }
            if (ss->accumulating == 2) ss->accumulating = 1;
        }

        if (showstats_display)
        {
            /* We don't want to depend on glWindowPos since it
             * needs OpenGL 1.4, but fortunately the aux context
             * has identity MVP matrix.
             */
            CALL_glPushAttrib(GL_CURRENT_BIT | GL_VIEWPORT_BIT);
            CALL_glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            CALL_glRasterPos2f(-0.9, 0.9);
            showstats_render(ss, showstats_display);
            CALL_glPopAttrib();
        }

        CALL_glPushAttrib(GL_VIEWPORT_BIT);
        CALL_glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        CALL_glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        CALL_glEnable(GL_TEXTURE_1D);
        for (i = bugle_list_head(&showstats_stats); i; i = bugle_list_next(i))
        {
            sst = (showstats_statistic *) bugle_list_data(i);
            if (sst->mode == SHOWSTATS_GRAPH && sst->graph_tex)
            {
                GLfloat s;

                s = (GLfloat) sst->graph_offset / sst->graph_size;
                CALL_glBindTexture(GL_TEXTURE_1D, sst->graph_tex);
                CALL_glBegin(GL_QUADS);
                CALL_glTexCoord3f(s, 0.0f, 0.0f);
                CALL_glVertex2f(-0.95f, -0.95f);
                CALL_glTexCoord3f(s + 1.0f, 0.0f, 0.0f);
                CALL_glVertex2f(-0.6f, -0.95f);
                CALL_glTexCoord3f(s + 1.0f, 0.0f, 1.0f);
                CALL_glVertex2f(-0.6f, -0.75f);
                CALL_glTexCoord3f(s, 0.0f, 1.0f);
                CALL_glVertex2f(-0.95f, -0.75f);
                CALL_glEnd();
            }
        }
        CALL_glDisable(GL_TEXTURE_1D);
        CALL_glBindTexture(GL_TEXTURE_1D, 0);
        CALL_glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
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
static bool showstats_show_set(const struct filter_set_variable_info_s *var,
                               const char *text, const void *value)
{
    showstats_statistic *sst;

    sst = (showstats_statistic *) bugle_calloc(1, sizeof(showstats_statistic));
    sst->name = bugle_strdup(text);
    sst->mode = SHOWSTATS_TEXT;
    bugle_list_append(&showstats_stats, sst);
    return true;
}

/* Same as above but for graphing */
static bool showstats_graph_set(const struct filter_set_variable_info_s *var,
                                const char *text, const void *value)
{
    showstats_statistic *sst;

    sst = (showstats_statistic *) bugle_calloc(1, sizeof(showstats_statistic));
    sst->name = bugle_strdup(text);
    sst->mode = SHOWSTATS_GRAPH;
    bugle_list_append(&showstats_stats, sst);
    return true;
}

static bool showstats_initialise(filter_set *handle)
{
    filter *f;
    bugle_list_node *i;
    showstats_statistic *sst;

    f = bugle_register_filter(handle, "showstats");
    bugle_register_filter_depends("invoke", "showstats");
    bugle_register_filter_depends("screenshot", "showstats");
    /* make sure that screenshots capture the stats */
    bugle_register_filter_depends("debugger", "showstats");
    bugle_register_filter_depends("screenshot", "showstats");
    bugle_register_filter_depends("showstats", "stats");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, showstats_glXSwapBuffers);
    showstats_view = bugle_object_class_register(&bugle_context_class,
                                                 showstats_struct_initialise,
                                                 showstats_struct_destroy,
                                                 sizeof(showstats_struct));

    /* Value of arg is irrelevant, only truth value */
    bugle_register_xevent_key(&key_showstats_accumulate, NULL,
                              showstats_accumulate_callback, f);
    bugle_register_xevent_key(&key_showstats_noaccumulate, NULL,
                              showstats_accumulate_callback, NULL);


    for (i = bugle_list_head(&showstats_stats); i; i = bugle_list_next(i))
    {
        sst = (showstats_statistic *) bugle_list_data(i);
        sst->st = stats_statistic_find(sst->name);
        if (!sst->st)
        {
            fprintf(stderr, "Statistic '%s' not found.\n", sst->name);
            stats_statistic_list("showstats");
            return false;
        }
        else
            stats_expression_activate_signals(sst->st->value);
    }
    stats_signal_values_init(&showstats_prev);
    stats_signal_values_init(&showstats_cur);

    return true;
}

static void showstats_destroy(filter_set *handle)
{
    bugle_list_node *i;
    showstats_statistic *sst;

    free(showstats_display);
    stats_signal_values_clear(&showstats_prev);
    stats_signal_values_clear(&showstats_cur);
    for (i = bugle_list_head(&showstats_stats); i; i = bugle_list_next(i))
    {
        sst = (showstats_statistic *) bugle_list_data(i);
        free(sst->name);
    }
    bugle_list_clear(&showstats_stats);
}

/*** Global initialisation */

void bugle_initialise_filter_library(void)
{
    static const filter_set_info stats_info =
    {
        "stats",
        stats_initialise,
        stats_destroy,
        NULL,
        NULL,
        NULL,
        0,
        NULL
    };

    static const filter_set_info stats_basic_info =
    {
        "stats_basic",
        stats_basic_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        "stats module: frames and timing"
    };

    static const filter_set_info stats_primitives_info =
    {
        "stats_primitives",
        stats_primitives_initialise,
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
        stats_fragments_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        "stats module: fragments that pass the depth test"
    };
#endif

#if HAVE_NVPERFSDK_H
    static const filter_set_variable_info stats_nv_variables[] =
    {
        { "flush", "flush OpenGL after each frame (more accurate but slower)", FILTER_SET_VARIABLE_BOOL, &stats_nv_flush, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info stats_nv_info =
    {
        "stats_nv",
        stats_nv_initialise,
        stats_nv_destroy,
        NULL,
        NULL,
        stats_nv_variables,
        0,
        "stats module: get counters from NVPerfSDK"
    };
#endif /* HAVE_NVPERFSDK_H */

    static const filter_set_variable_info logstats_variables[] =
    {
        { "show", "repeat with each item to log", FILTER_SET_VARIABLE_CUSTOM, NULL, logstats_show_set },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info logstats_info =
    {
        "logstats",
        logstats_initialise,
        logstats_destroy,
        NULL,
        NULL,
        logstats_variables,
        0,
        "reports statistics to the log"
    };

    static const filter_set_variable_info showstats_variables[] =
    {
        { "show", "repeat with each item to render", FILTER_SET_VARIABLE_CUSTOM, NULL, showstats_show_set },
        { "graph", "repeat with each item to graph", FILTER_SET_VARIABLE_CUSTOM, NULL, showstats_graph_set },
        { "key_accumulate", "frame rate is averaged from time key is pressed [none]", FILTER_SET_VARIABLE_KEY, &key_showstats_accumulate, NULL },
        { "key_noaccumulate", "return frame rate to instantaneous display [none]", FILTER_SET_VARIABLE_KEY, &key_showstats_noaccumulate, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info showstats_info =
    {
        "showstats",
        showstats_initialise,
        showstats_destroy,
        NULL,
        NULL,
        showstats_variables,
        0,
        "renders statistics onto the screen"
    };

    bugle_register_filter_set(&stats_info);

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

#if HAVE_NVPERFSDK_H
    bugle_register_filter_set(&stats_nv_info);
    bugle_register_filter_set_depends("stats_nv", "stats");
    bugle_register_filter_set_depends("stats_nv", "stats_basic");
#endif

    bugle_register_filter_set(&logstats_info);
    bugle_register_filter_set_depends("logstats", "stats");
    bugle_register_filter_set_depends("logstats", "log");
    bugle_list_init(&logstats_show_requested, true);

    bugle_register_filter_set(&showstats_info);
    bugle_register_filter_set_depends("showstats", "stats");
    bugle_register_filter_set_depends("showstats", "trackextensions");
    bugle_register_filter_set_renders("showstats");
    bugle_list_init(&showstats_stats, true);
}
