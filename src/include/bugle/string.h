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

/* Abstraction layer around POSIX/C99/GNU string functions */

#ifndef BUGLE_STRING_H
#define BUGLE_STRING_H

#include <bugle/attributes.h>
#include <bugle/export.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

BUGLE_EXPORT_PRE int bugle_snprintf(char *str, size_t size, const char *format, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(3, 4) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int bugle_vsnprintf(char *str, size_t size, const char *format, va_list ap) BUGLE_ATTRIBUTE_FORMAT_PRINTF(3, 0) BUGLE_EXPORT_POST;

/* These correspond to the xasprintf function in gnulib - they return the
 * allocated memory, and do not return if allocation failed.
 */
BUGLE_EXPORT_PRE char *bugle_asprintf(const char *format, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 2) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE char *bugle_vasprintf(const char *format, va_list ap) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 0) BUGLE_EXPORT_POST;

/* Calls bugle_alloc_die on alloc failure */
BUGLE_EXPORT_PRE char *bugle_strdup(const char *str) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE char *bugle_strndup(const char *str, size_t n) BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* BUGLE_STRING_H */
