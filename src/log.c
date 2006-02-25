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
#define _POSIX_SOURCE /* for flockfile */
#include "src/filters.h"
#include "common/safemem.h"
#include "common/bool.h"
#include <stdio.h>

static char *log_filename = NULL;
static bool log_flush = false;
static FILE *log_file;

FILE *bugle_log_header(const char *filterset, const char *mode)
{
    if (!log_file) return NULL;
    if (mode) fprintf(log_file, "%s.%s: ", filterset, mode);
    else fprintf(log_file, "%s: ", filterset);
    return log_file;
}

static bool log_pre_callback(function_call *call, const callback_data *data)
{
    if (log_file) flockfile(log_file);
    return true;
}

static bool log_post_callback(function_call *call, const callback_data *data)
{
    if (log_file)
    {
        if (log_flush) fflush(log_file);
        funlockfile(log_file);
    }
    return true;
}

static bool initialise_log(filter_set *handle)
{
    filter *f;

    if (log_filename)
        log_file = fopen(log_filename, "w");
    else
        log_file = stderr;
    if (!log_file)
    {
        if (log_filename)
            fprintf(stderr, "failed to open log file %s\n", log_filename);
        return false;
    }
    f = bugle_register_filter(handle, "log_pre");
    bugle_register_filter_catches_all(f, false, log_pre_callback);
    f = bugle_register_filter(handle, "log_post");
    bugle_register_filter_catches_all(f, false, log_post_callback);
    return true;
}

static void destroy_log(filter_set *handle)
{
    if (log_filename)
    {
        if (log_file) fclose(log_file);
        free(log_filename);
    }
}

void bugle_log_register_filter(const char *filter)
{
    bugle_register_filter_depends(filter, "log_pre");
    bugle_register_filter_depends("log_post", filter);
}

void log_initialise(void)
{
    static const filter_set_variable_info log_variables[] =
    {
        { "filename", "filename of the log to write [stderr]", FILTER_SET_VARIABLE_STRING, &log_filename, NULL },
        { "flush", "flush log after every call [no]", FILTER_SET_VARIABLE_BOOL, &log_flush, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info log_info =
    {
        "log",
        initialise_log,
        destroy_log,
        NULL,
        NULL,
        log_variables,
        0,
        "provides logging services to other filter-sets"
    };

    bugle_register_filter_set(&log_info);
}
