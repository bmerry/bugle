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
#include "common/bool.h"
#include "common/safemem.h"
#include "src/stats.h"
#include "src/utils.h"
#include "src/filters.h"

static stats_signal *stats_calls_counts[NUMBER_OF_FUNCTIONS];

static bool stats_calls_callback(function_call *call, const callback_data *data)
{
    bugle_stats_signal_add(stats_calls_counts[call->generic.id], 1.0);
    return true;
}

static bool stats_calls_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_register(handle, "stats_calls");
    bugle_filter_catches_all(f, false, stats_calls_callback);

    for (int i = 0; i < NUMBER_OF_FUNCTIONS; i++)
    {
        char *name;
        bugle_asprintf(&name, "calls:%s", budgie_function_table[i].name);
        stats_calls_counts[i] = bugle_stats_signal_register(name, NULL, NULL);
        free(name);
    }
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info stats_calls_info =
    {
        "stats_calls",
        stats_calls_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "stats module: call counts"
    };

    bugle_filter_set_register(&stats_calls_info);
    bugle_filter_set_stats_generator("stats_calls");
}
