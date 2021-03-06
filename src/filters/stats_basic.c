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
#include <bugle/bool.h>
#include <bugle/time.h>
#include <bugle/stats.h>
#include <bugle/filters.h>
#include <bugle/glwin/glwin.h>

static stats_signal *stats_basic_frames, *stats_basic_seconds;

static bugle_bool stats_basic_seconds_activate(stats_signal *si)
{
    bugle_timespec t;
    bugle_gettime(&t);
    bugle_stats_signal_update(si, t.tv_sec + 1e-9 * t.tv_nsec);
    return BUGLE_TRUE;
}

static bugle_bool stats_basic_swap_buffers(function_call *call, const callback_data *data)
{
    bugle_timespec now;

    bugle_gettime(&now);
    bugle_stats_signal_update(stats_basic_seconds, now.tv_sec + 1e-9 * now.tv_nsec);
    bugle_stats_signal_add(stats_basic_frames, 1.0);
    return BUGLE_TRUE;
}

static bugle_bool stats_basic_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "stats_basic");
    bugle_glwin_filter_catches_swap_buffers(f, BUGLE_FALSE, stats_basic_swap_buffers);
    bugle_filter_order("stats_basic", "invoke");
    bugle_filter_order("stats_basic", "stats");

    stats_basic_frames = bugle_stats_signal_new("frames", NULL, NULL);
    stats_basic_seconds = bugle_stats_signal_new("seconds", NULL, stats_basic_seconds_activate);
    return BUGLE_TRUE;
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

    bugle_filter_set_new(&stats_basic_info);
    bugle_filter_set_stats_generator("stats_basic");
}
