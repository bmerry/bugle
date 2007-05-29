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
#include <assert.h>

static char *log_filename = NULL;
static char *log_format = NULL;
static bool log_flush = false;
static FILE *log_file = NULL;
static int log_file_level = BUGLE_LOG_INFO + 1;
static int log_stderr_level = BUGLE_LOG_NOTICE + 1;

static const char *log_level_names[] =
{
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

static inline void log_start(FILE *f)
{
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
    flockfile(f);
#endif
}

static inline void log_end(FILE *f)
{
    if (log_flush) fflush(f);
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
    funlockfile(f);
#endif
}

/* Processes tokens from *format until it hits something it cannot
 * handle itself (i.e., %m). The return value indicates what was hit:
 * 0: all done
 * 1: %m (format is advanced past the %m)
 */
static int log_next(FILE *f, const char **format, const char *filterset, const char *event, int severity)
{
    while (**format)
    {
        if (**format == '%')
        {
            switch ((*format)[1])
            {
            case 'l': fputs(log_level_names[severity], f); break;
            case 'f': fputs(filterset, f); break;
            case 'e': fputs(event, f); break;
            case 'm': *format += 2; return 1;
            case 'p': fprintf(f, "%lu", (unsigned long) getpid()); break;
            case 't': fprintf(f, "%lu", bugle_thread_self()); break;
            case '%': fputc('%', f); break;
            default: /* Unrecognised escape, treat it as literal */
                fputc('%', f);
                (*format)--;
            }
            *format += 2;
        }
        else
        {
            fputc(**format, f);
            (*format)++;
        }
    }
    fputc('\n', f);
    return 0;
}

void bugle_log_callback(const char *filterset, const char *event, int severity,
                               void (*callback)(void *arg, FILE *f), void *arg)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        const char *format;
        int special;
        FILE *f;
        int level;

        f = i ? stderr : log_file;
        level = i ? log_stderr_level : log_file_level;
        if (!f || severity >= level) continue;

        log_start(f);
        format = log_format;
        while ((special = log_next(f, &format, filterset, event, severity)) != 0)
            switch (special)
            {
            case 1:
                (*callback)(arg, f);
                break;
            }
        log_end(f);
    }
}

void bugle_log_printf(const char *filterset, const char *event, int severity,
                      const char *msg_format, ...)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        va_list ap;
        const char *format;
        int special;
        FILE *f;
        int level;

        f = i ? stderr : log_file;
        level = i ? log_stderr_level : log_file_level;
        if (!f || severity >= level) continue;

        log_start(f);
        format = log_format;
        while ((special = log_next(f, &format, filterset, event, severity)) != 0)
            switch (special)
            {
            case 1:
                va_start(ap, msg_format);
                vfprintf(f, msg_format, ap);
                va_end(ap);
                break;
            }
        log_end(f);
    }
}

void bugle_log(const char *filterset, const char *event, int severity,
               const char *message)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        const char *format;
        int special;
        FILE *f;
        int level;

        f = i ? stderr : log_file;
        level = i ? log_stderr_level : log_file_level;
        if (!f || severity >= level) continue;

        log_start(f);
        format = log_format;
        while ((special = log_next(f, &format, filterset, event, severity)) != 0)
            switch (special)
            {
            case 1:
                fputs(message, f);
                break;
            }
        log_end(f);
    }
}

static bool initialise_log(filter_set *handle)
{
    if (log_filename)
    {
        log_file = fopen(log_filename, "w");
        if (!log_file)
        {
            fprintf(stderr, "failed to open log file %s\n", log_filename);
            return false;
        }
    }
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

void log_initialise(void)
{
    static const filter_set_variable_info log_variables[] =
    {
        { "filename", "filename of the log to write [none]", FILTER_SET_VARIABLE_STRING, &log_filename, NULL },
        { "flush", "flush log after every call [no]", FILTER_SET_VARIABLE_BOOL, &log_flush, NULL },
        { "format", "template for log lines [[%l] %f.%e: %m]", FILTER_SET_VARIABLE_STRING, &log_format, NULL },
        { "file_level", "how much information to log to file [4] (0 is none, 5 is all)", FILTER_SET_VARIABLE_UINT, &log_file_level, NULL },
        { "stderr_level", "how much information to log to stderr [3]", FILTER_SET_VARIABLE_UINT, &log_stderr_level, NULL },
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
