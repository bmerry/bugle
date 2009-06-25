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

#ifndef BUGLE_PLATFORM_DL_H
#define BUGLE_PLATFORM_DL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <budgie/types.h>
#include <bugle/export.h>

#define BUGLE_DL_FLAG_GLOBAL   1    /* default is local if there is a choice */
#define BUGLE_DL_FLAG_NOW      2    /* default is lazy if there is a choice */
#define BUGLE_DL_FLAG_SUFFIX   4    /* append platform-specific file suffix first */


typedef void *bugle_dl_module;

/* Open the module; returns NULL on error.
 * filename must be non-NULL
 */
BUGLE_EXPORT_PRE bugle_dl_module bugle_dl_open(const char *filename, int flag) BUGLE_EXPORT_POST;

/* Closes the module. Returns 0 on success. Modules are reference counted
 * for open/close.
 */
BUGLE_EXPORT_PRE int bugle_dl_close(bugle_dl_module module) BUGLE_EXPORT_POST;

/* Retrieves a pointer to data. module must be non-NULL. Returns NULL on
 * error.
 */
BUGLE_EXPORT_PRE void *bugle_dl_sym_data(bugle_dl_module module, const char *symbol) BUGLE_EXPORT_POST;

/* Retrieves a pointer to code. module must be non-NULL. Returns NULL on
 * error.
 */
BUGLE_EXPORT_PRE BUDGIEAPIPROC bugle_dl_sym_function(bugle_dl_module module, const char *symbol) BUGLE_EXPORT_POST;

/*** The remaining functions should only be used by the filter system ***/

/* Initialise module system */
BUGLE_EXPORT_PRE void bugle_dl_init(void);

/* For each module in path, call the callback function with the path to the
 * file.
 */
void bugle_dl_foreach(const char *path, void (*callback)(const char *filename, void *arg), void *arg);

/* Indicate that initialisation is done and future user calls to dlopen should
 * be intercepted if possible to handle apps that dynamically load libGL.
 */
void bugle_dl_enable_interception(void);

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_PLATFORM_DL_H */
