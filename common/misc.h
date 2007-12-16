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

/* Miscellaneous utility functions that don't fit anywhere else */

#ifndef BUGLE_COMMON_SAFEMEM_H
#define BUGLE_COMMON_SAFEMEM_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include <stdio.h>

/* Appends to *strp using the format, reallocating if necessary. The
 * current size is *sz, and will be updated on reallocation. *strp may
 * be NULL, in which case this reduces to asprintf. The return value is
 * the length of the string, including the original portion. The string
 * length is at least doubled each time to ensure progress is made.
 */
int bugle_appendf(char **strp, size_t *sz, const char *format, ...) BUGLE_GCC_FORMAT_PRINTF(3, 4);

/* Like fgets, but creates the memory. The return value has the same
 * meaning as for fgets, but must be free()ed if non-NULL
 */
char *bugle_afgets(FILE *stream);

#endif /* !BUGLE_COMMON_SAFEMEM_H */
