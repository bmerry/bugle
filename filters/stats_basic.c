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
#include <stdbool.h>
#include <sys/time.h>
#include <sys/types.h>
#include <bugle/stats.h>
#include <bugle/filters.h>

static stats_signal *stats_basic_frames, *stats_basic_seconds;

static bool stats_basic_seconds_activate(stats_signal *si)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    bugle_stats_signal_update(si, t.tv_sec + 1e-6 * t.tv_usec);
    return true;
}

static bool stats_basic_glXSwapBuffers(function_call *call, const callback_data *data)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    bugle_stats_signal_update(stats_basic_seconds, now.tv_sec + 1e-6 * now.tv_usec);
    bugle_stats_signal_add(stats_basic_frames, 1.0);
    return true;
}

static bool stats_basic_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_register(handle, "stats_basic");
    bugle_filter_catches(f, "glXSwapBuffers", false, stats_basic_glXSwapBuffers);
    bugle_filter_order("stats_basic", "invoke");
    bugle_filter_order("stats_basic", "stats");

    stats_basic_frames = bugle_stats_signal_register("frames", NULL, NULL);
    stats_basic_seconds = bugle_stats_signal_register("seconds", NULL, stats_basic_seconds_activate);
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info stats_basic_info =
    {
        "stats_basic",
        stats_basic_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "stats module: frames and timing"
    };

    bugle_filter_set_register(&stats_basic_info);
    bugle_filter_set_stats_generator("stats_basic");
}
