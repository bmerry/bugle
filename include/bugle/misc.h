/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007, 2009  Bruce Merry
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

#ifndef BUGLE_COMMON_MISC_H
#define BUGLE_COMMON_MISC_H

#include <stddef.h>
#include <stdio.h>
#include <bugle/attributes.h>
#include <bugle/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Appends to *strp using the format, reallocating if necessary. The
 * current size is *sz, and will be updated on reallocation. *strp may
 * be NULL, in which case this reduces to asprintf. The return value is
 * the length of the string, including the original portion. The string
 * length is at least doubled each time to ensure progress is made.
 */
BUGLE_EXPORT_PRE int bugle_appendf(char **strp, size_t *sz, const char *format, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(3, 4) BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_COMMON_MISC_H */
