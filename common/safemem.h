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

/* The function names and functionality are modelled on libiberty,
 * but are independently implemented. They are light wrappers around
 * the similarly named standard functions, but they display an error
 * and exit if memory could not be allocated. In some cases they also
 * reimplement non-portable functionality.
 */

#ifndef BUGLE_COMMON_SAFEMEM_H
#define BUGLE_COMMON_SAFEMEM_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Appends src to dest, possibly bugle_realloc()ing, and returns the pointer.
 * It assumes that dest was malloc()ed with a size that is the smallest
 * power of 2 large enough to hold it, and this will also hold as a
 * post-condition.
 */
char *bugle_strcat(char *dest, const char *src);

/* Appends to *strp using the format, reallocating if necessary. The
 * current size is *sz, and will be updated on reallocation. *strp may
 * be NULL, in which case this reduces to asprintf. The return value is
 * the length of the string, including the original portion.
 */
int bugle_appendf(char **strp, size_t *sz, const char *format, ...);

/* Like fgets, but creates the memory. The return value has the same
 * meaning as for fgets, but must be free()ed if non-NULL
 */
char *bugle_afgets(FILE *stream);

/* Only sort of memory related: a shutdown handler that has an unlimited
 * number of entries and will pass arbitrary pointers to the shutdown
 * functions.
 */
void bugle_atexit(void (*shutdown_function)(void *), void *arg);

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_COMMON_SAFEMEM_H */
