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

/* Provides a callback to indicate memory allocation failure, which uses
 * the logging system if available.
 */

#ifndef BUGLE_SRC_DIE_H
#define BUGLE_SRC_DIE_H

#include <bugle/export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bugle_alloc_die_callback)(void);

/* Obtain the current callback (might be NULL) */
BUGLE_EXPORT_PRE bugle_alloc_die_callback bugle_get_alloc_die(void) BUGLE_EXPORT_POST;

/* Set the current callback */
BUGLE_EXPORT_PRE void bugle_set_alloc_die(bugle_alloc_die_callback callback) BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_SRC_DIE_H */
