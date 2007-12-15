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

/* Each message written to the log has three associated attributes:
 * - the filterset that generated it
 * - a submodule of the filterset (filterset dependent)
 * - a severity, which determines whether it is actually logged
 * The different logging methods all take these as parameters.
 */

#ifndef BUGLE_SRC_LOG_H
#define BUGLE_SRC_LOG_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>
#include "src/utils.h"
#include "src/filters.h"

enum
{
    BUGLE_LOG_ERROR = 0,
    BUGLE_LOG_WARNING,
    BUGLE_LOG_NOTICE,
    BUGLE_LOG_INFO,
    BUGLE_LOG_DEBUG
};

/* Write a message to the log. Do not include a trailing newline; this will
 * be provided automatically.
 */
void bugle_log(const char *filterset, const char *event, int severity,
               const char *message);

/* Variant of bugle_log with printf-style semantics */
void bugle_log_printf(const char *filterset, const char *event, int severity,
                      const char *format, ...) BUGLE_GCC_FORMAT_PRINTF(4, 5);

/* A generalised version that takes a callback to write out to the log file.
 * The callback must be prepared to deal with zero or more calls. Again, it
 * should not output a newline.
 */
void bugle_log_callback(const char *filterset, const char *event, int severity,
                        void (*callback)(void *arg, FILE *f), void *arg);

/* An implementation of xalloc_die. The actual xalloc_die symbol is in
 * src/xalloc-die-log.c.
 */
void bugle_log_xalloc_die(void);

/* Used internally by the initialisation code */
void log_initialise(void);

#endif /* !BUGLE_SRC_LOG_H */
