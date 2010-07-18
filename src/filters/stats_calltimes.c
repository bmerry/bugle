/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007, 2009  Bruce Merry
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
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include <bugle/time.h>
#include <bugle/stats.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <budgie/reflect.h>

static stats_signal **stats_calltimes_signals;
static stats_signal *stats_calltimes_total;
static object_view time_view;

static bugle_bool stats_calltimes_pre(function_call *call, const callback_data *data)
{
    bugle_timespec *start;

    start = bugle_object_get_current_data(bugle_get_call_class(), time_view);
    bugle_gettime(start);
    return BUGLE_TRUE;
}

static bugle_bool stats_calltimes_post(function_call *call, const callback_data *data)
{
    bugle_timespec *start;
    bugle_timespec end;
    double elapsed;

    bugle_gettime(&end);
    start = bugle_object_get_current_data(bugle_get_call_class(), time_view);
    elapsed = (end.tv_sec  - start->tv_sec) + 1e-9 * (end.tv_nsec - start->tv_nsec);

    bugle_stats_signal_add(stats_calltimes_signals[call->generic.id], elapsed);
    bugle_stats_signal_add(stats_calltimes_total, elapsed);
    return BUGLE_TRUE;
}

static bugle_bool stats_calltimes_initialise(filter_set *handle)
{
    filter *f;
    int i;

    f = bugle_filter_new(handle, "stats_calltimes_pre");
    bugle_filter_catches_all(f, BUGLE_FALSE, stats_calltimes_pre);
    bugle_filter_order("stats_calltimes_pre", "invoke");
    bugle_filter_order("stats_calltimes_pre", "stats");

    f = bugle_filter_new(handle, "stats_calltimes_post");
    bugle_filter_catches_all(f, BUGLE_FALSE, stats_calltimes_post);
    bugle_filter_order("invoke", "stats_calltimes_post");

    /* Try to get this filter-set close to the calls */
    bugle_filter_order("stats_calls", "stats_calltimes_pre");
    bugle_filter_order("stats_basic", "stats_calltimes_pre");
    bugle_filter_order("stats_primitives", "stats_calltimes_pre");

    stats_calltimes_signals = BUGLE_NMALLOC(budgie_function_count(), stats_signal *);
    for (i = 0; i < budgie_function_count(); i++)
    {
        char *name;
        name = bugle_asprintf("calltimes:%s", budgie_function_name(i));
        stats_calltimes_signals[i] = bugle_stats_signal_new(name, NULL, NULL);
        bugle_free(name);
    }
    stats_calltimes_total = bugle_stats_signal_new("calltimes:total", NULL, NULL);

    time_view = bugle_object_view_new(bugle_get_call_class(),
                                      NULL,
                                      NULL,
                                      sizeof(bugle_timespec));

    return BUGLE_TRUE;
}

static void stats_calltimes_shutdown(filter_set *handle)
{
    bugle_free(stats_calltimes_signals);
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info stats_calltimes_info =
    {
        "stats_calltimes",
        stats_calltimes_initialise,
        stats_calltimes_shutdown,
        NULL,
        NULL,
        NULL,
        "stats module: measure times of calls"
    };

    bugle_filter_set_new(&stats_calltimes_info);
    bugle_filter_set_stats_generator("stats_calltimes");
}
