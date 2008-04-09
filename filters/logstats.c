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
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <bugle/linkedlist.h>
#include <bugle/stats.h>
#include <bugle/filters.h>
#include <bugle/glwin.h>
#include <bugle/log.h>
#include "xalloc.h"

static linked_list logstats_show;    /* actual stats */
static linked_list logstats_show_requested;  /* names in config file */
static stats_signal_values logstats_prev, logstats_cur;

/* Callback to assign the "show" pseudo-variable */
static bool logstats_show_set(const struct filter_set_variable_info_s *var,
                              const char *text, const void *value)
{
    bugle_list_append(&logstats_show_requested, xstrdup(text));
    return true;
}

static bool logstats_swap_buffers(function_call *call, const callback_data *data)
{
    linked_list_node *i;
    stats_statistic *st;
    stats_signal_values tmp;
    stats_substitution *sub;

    tmp = logstats_prev;
    logstats_prev = logstats_cur;
    logstats_cur = tmp;
    bugle_stats_signal_values_gather(&logstats_cur);
    if (logstats_prev.allocated)
    {
        for (i = bugle_list_head(&logstats_show); i; i = bugle_list_next(i))
        {
            st = (stats_statistic *) bugle_list_data(i);
            double v = bugle_stats_expression_evaluate(st->value, &logstats_prev, &logstats_cur);
            if (FINITE(v))
            {
                sub = bugle_stats_statistic_find_substitution(st, v);
                if (sub)
                    bugle_log_printf("logstats", st->name, BUGLE_LOG_INFO,
                                     "%s %s",
                                     sub->replacement,
                                     st->label ? st->label : "");
                else
                    bugle_log_printf("logstats", st->name, BUGLE_LOG_INFO,
                                     "%.*f %s",
                                     st->precision, v,
                                     st->label ? st->label : "");
            }
        }
    }
    return true;
}

static bool logstats_initialise(filter_set *handle)
{
    filter *f;
    linked_list_node *i, *j;
    stats_statistic *st;

    f = bugle_filter_new(handle, "stats_log");
    bugle_glwin_filter_catches_swap_buffers(f, false, logstats_swap_buffers);

    bugle_list_clear(&logstats_show);
    for (i = bugle_list_head(&logstats_show_requested); i; i = bugle_list_next(i))
    {
        char *name;
        name = (char *) bugle_list_data(i);
        j = bugle_stats_statistic_find(name);
        if (!j)
        {
            bugle_log_printf("logstats", "initialise", BUGLE_LOG_ERROR,
                             "statistic '%s' not found.", name);
            bugle_stats_statistic_list();
            return false;
        }
        for (; j; j = bugle_list_next(j))
        {
            st = (stats_statistic *) bugle_list_data(j);
            if (bugle_stats_expression_activate_signals(st->value))
                bugle_list_append(&logstats_show, st);
            else
            {
                bugle_log_printf("logstats", "initialise", BUGLE_LOG_ERROR,
                                 "could not initialise statistic '%s'",
                                 st->name);
                return false;
            }
            if (st->last) break;
        }
    }
    bugle_list_clear(&logstats_show_requested);
    bugle_stats_signal_values_init(&logstats_prev);
    bugle_stats_signal_values_init(&logstats_cur);
    return true;
}

static void logstats_shutdown(filter_set *handle)
{
    bugle_stats_signal_values_clear(&logstats_prev);
    bugle_stats_signal_values_clear(&logstats_cur);
    bugle_list_clear(&logstats_show);
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info logstats_variables[] =
    {
        { "show", "repeat with each item to log", FILTER_SET_VARIABLE_CUSTOM, NULL, logstats_show_set },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info logstats_info =
    {
        "logstats",
        logstats_initialise,
        logstats_shutdown,
        NULL,
        NULL,
        logstats_variables,
        "reports statistics to the log"
    };

    bugle_filter_set_new(&logstats_info);
    bugle_filter_set_stats_logger("logstats");
    bugle_list_init(&logstats_show_requested, free);
}
