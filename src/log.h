/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/* When logging stuff, you should call log_header before each line of output.
 * If the logger is loaded, it will write a prefix and return a file handle.
 * Otherwise, it will return NULL and you should not continue.
 *
 * You should also call log_register_filter to set up the order dependencies
 * on any filter that will do logging.
 */
FILE *bugle_log_header(const char *filterset, const char *mode);

/* Call this for filter sets that use logging */
void bugle_log_register_filter(const char *filter);

/* Used by the initialisation code */
void log_initialise(void);

#endif /* !BUGLE_SRC_LOG_H */
