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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include <bugle/stats.h>
#include <bugle/filters.h>
#include <budgie/reflect.h>

static stats_signal **stats_calls_counts;

static bugle_bool stats_calls_callback(function_call *call, const callback_data *data)
{
    bugle_stats_signal_add(stats_calls_counts[call->generic.id], 1.0);
    return BUGLE_TRUE;
}

static bugle_bool stats_calls_initialise(filter_set *handle)
{
    filter *f;
    int i;

    f = bugle_filter_new(handle, "stats_calls");
    bugle_filter_catches_all(f, BUGLE_FALSE, stats_calls_callback);

    stats_calls_counts = BUGLE_NMALLOC(budgie_function_count(), stats_signal *);
    for (i = 0; i < budgie_function_count(); i++)
    {
        char *name;
        name = bugle_asprintf("calls:%s", budgie_function_name(i));
        stats_calls_counts[i] = bugle_stats_signal_new(name, NULL, NULL);
        bugle_free(name);
    }
    return BUGLE_TRUE;
}

static void stats_calls_shutdown(filter_set *handle)
{
    bugle_free(stats_calls_counts);
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info stats_calls_info =
    {
        "stats_calls",
        stats_calls_initialise,
        stats_calls_shutdown,
        NULL,
        NULL,
        NULL,
        "stats module: call counts"
    };

    bugle_filter_set_new(&stats_calls_info);
    bugle_filter_set_stats_generator("stats_calls");
}
