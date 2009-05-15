/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009  Bruce Merry
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

#ifndef BUGLE_MEMORY_H
#define BUGLE_MEMORY_H

#include <stdlib.h>
#include <bugle/attributes.h>
#include <bugle/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/* All the allocation functions below will call bugle_alloc_die (which
 * in turn provides a user-provided callback) on an allocation failure.
 * Thus, it is not necessary to check the return value.
 */

BUGLE_EXPORT_PRE void *bugle_malloc(size_t size) BUGLE_ATTRIBUTE_MALLOC BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE void *bugle_calloc(size_t nmemb, size_t size) BUGLE_ATTRIBUTE_MALLOC BUGLE_EXPORT_POST;

/* Like calloc, but does not zero. */
BUGLE_EXPORT_PRE void *bugle_nmalloc(size_t nmemb, size_t size) BUGLE_ATTRIBUTE_MALLOC BUGLE_EXPORT_POST;

/* Like malloc, but zeros. */
BUGLE_EXPORT_PRE void *bugle_zalloc(size_t size) BUGLE_ATTRIBUTE_MALLOC BUGLE_EXPORT_POST;

/* Like realloc, but with a count */
BUGLE_EXPORT_PRE void *bugle_nrealloc(void *ptr, size_t nmemb, size_t size) BUGLE_EXPORT_POST;

#define BUGLE_MALLOC(type) ((type *) bugle_malloc(sizeof(type)))
#define BUGLE_NMALLOC(count, type) ((type *) bugle_nmalloc(sizeof(type), count))
#define BUGLE_CALLOC(count, type) ((type *) bugle_calloc(sizeof(type), count))
#define BUGLE_ZALLOC(type) ((type *) bugle_zalloc(sizeof(type)))

typedef void (*bugle_alloc_die_callback)(void);

/* Obtain the current callback (might be NULL) */
BUGLE_EXPORT_PRE bugle_alloc_die_callback bugle_get_alloc_die(void) BUGLE_EXPORT_POST;

/* Set the current callback */
BUGLE_EXPORT_PRE void bugle_set_alloc_die(bugle_alloc_die_callback callback) BUGLE_EXPORT_POST;

/* Trigger the callback */
BUGLE_EXPORT_PRE void bugle_alloc_die(void) BUGLE_ATTRIBUTE_NORETURN BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_MEMORY_H */
