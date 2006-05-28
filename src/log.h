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

#ifndef BUGLE_SRC_LOG_H
#define BUGLE_SRC_LOG_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "common/bool.h"
#include "src/utils.h"
#include "src/filters.h"

/* Write a message to the log. Do not include a trailing newline; this will
 * be provided automatically.
 */
void bugle_log(const char *filterset, const char *event, const char *message);

/* A generalised version that takes a callback to write out to the log file.
 * The callback must be prepared to deal with zero or more calls. Again, it
 * should not output a newline.
 */
void bugle_log_callback(const char *filterset, const char *event,
                        void (*callback)(void *arg, FILE *f), void *arg);

/* Call this for filter sets that use logging */
void bugle_log_register_filter(const char *filter);

/* Used by the initialisation code */
void log_initialise(void);

#endif /* !BUGLE_SRC_LOG_H */
