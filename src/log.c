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
#define _POSIX_SOURCE /* for flockfile */
#include "src/log.h"
#include "src/filters.h"
#include "common/safemem.h"
#include "common/bool.h"
#include "common/threads.h"
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

static char *log_filename = NULL;
static char *log_format = NULL;
static bool log_flush = false;
static FILE *log_file = NULL;
static int log_level = BUGLE_LOG_INFO;

static const char *log_level_names[] =
{
    "DEBUG",
    "INFO",
    "NOTICE",
    "WARNING",
    "ERROR",
};

/* Processes tokens from *format until it hits something it cannot
 * handle itself (i.e., %m). The return value indicates what was hit:
 * 0: all done
 * 1: %m (format is advanced past the %m)
 */
static int log_next(const char **format, const char *filterset, const char *event, int severity)
{
    while (**format)
    {
        if (**format == '%')
        {
            switch ((*format)[1])
            {
            case 'l': fputs(log_level_names[severity], log_file); break;
            case 'f': fputs(filterset, log_file); break;
            case 'e': fputs(event, log_file); break;
            case 'm': *format += 2; return 1;
            case 'p': fprintf(log_file, "%lu", (unsigned long) getpid()); break;
            case 't': fprintf(log_file, "%lu", bugle_thread_self()); break;
            case '%': fputc('%', log_file); break;
            default: /* Unrecognised escape, treat it as literal */
                fputc('%', log_file);
                (*format)--;
            }
            *format += 2;
        }
        else
        {
            fputc(**format, log_file);
            (*format)++;
        }
    }
    fputc('\n', log_file);
    return 0;
}

void bugle_log_callback(const char *filterset, const char *event, int severity,
                        void (*callback)(void *arg, FILE *f), void *arg)
{
    const char *format;
    int special;

    if (!log_file || severity < log_level) return;
    format = log_format;
    while ((special = log_next(&format, filterset, event, severity)) != 0)
        switch (special)
        {
        case 1:
            (*callback)(arg, log_file);
            break;
        }
}

void bugle_log_printf(const char *filterset, const char *event, int severity,
                      const char *msg_format, ...)
{
    va_list ap;
    const char *format;
    int special;

    if (!log_file || severity < log_level) return;
    format = log_format;
    while ((special = log_next(&format, filterset, event, severity)) != 0)
        switch (special)
        {
        case 1:
            va_start(ap, msg_format);
            vfprintf(log_file, msg_format, ap);
            va_end(ap);
            break;
        }
}

void bugle_log(const char *filterset, const char *event, int severity,
               const char *message)
{
    const char *format;
    int special;

    if (!log_file || severity < log_level) return;
    format = log_format;
    while ((special = log_next(&format, filterset, event, severity)) != 0)
        switch (special)
        {
        case 1:
            fputs(message, log_file);
            break;
        }
}

static bool log_pre_callback(function_call *call, const callback_data *data)
{
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
    if (log_file) flockfile(log_file);
#endif
    return true;
}

static bool log_post_callback(function_call *call, const callback_data *data)
{
    if (log_file)
    {
        if (log_flush) fflush(log_file);
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
        funlockfile(log_file);
#endif
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
    free(log_format);
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
        { "format", "template for log lines [[%l] %f.%e: %m]", FILTER_SET_VARIABLE_STRING, &log_format, NULL },
        { "level", "how much information to log [1] (0 is most, 5 is none)", FILTER_SET_VARIABLE_UINT, &log_level, NULL },
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
    log_format = bugle_strdup("[%l] %f.%e: %m");
}
