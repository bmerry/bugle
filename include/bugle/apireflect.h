/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2008  Bruce Merry
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

#ifndef BUGLE_SRC_APIREFLECT_H
#define BUGLE_SRC_APIREFLECT_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>
#include <budgie/types.h>
#include <bugle/porting.h>
#include <bugle/export.h>

typedef unsigned int api_enum;
typedef int api_block;

#define NULL_EXTENSION_BLOCK (-1)
#define BUGLE_API_EXTENSION_BLOCK_GL 0
#define BUGLE_API_EXTENSION_BLOCK_GLWIN 1

/*** Extensions ***/
typedef int bugle_api_extension;
#define NULL_EXTENSION (-1)

/* Checks for GL extensions by #define from header files */
BUGLE_EXPORT_PRE int                 bugle_api_extension_count(void) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE const char *        bugle_api_extension_name(bugle_api_extension ext) BUGLE_EXPORT_POST;
/* The GL/GLX/etc core version represented, or NULL for actual extensions */
BUGLE_EXPORT_PRE const char *        bugle_api_extension_version(bugle_api_extension ext) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE api_block           bugle_api_extension_block(bugle_api_extension ext) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE bugle_api_extension bugle_api_extension_id(const char *name) BUGLE_EXPORT_POST;

/* Returns a pointer to a list terminated by NULL_EXTENSION. The storage is
 * static, so do not try to free it.
 */
BUGLE_EXPORT_PRE const bugle_api_extension *bugle_api_extension_group_members(bugle_api_extension ext) BUGLE_EXPORT_POST;

/*** Tokens ***/

BUGLE_EXPORT_PRE const char *bugle_api_enum_name(api_enum e, api_block block) BUGLE_EXPORT_POST;
/* Returns a list of extensions that define the token,
 * terminated by NULL_EXTENSION. This includes GL_VERSION_x_y "extensions".
 */
BUGLE_EXPORT_PRE const bugle_api_extension *bugle_api_enum_extensions(api_enum e, api_block block) BUGLE_EXPORT_POST;

/*** Functions ***/

BUGLE_EXPORT_PRE bugle_api_extension bugle_api_function_extension(budgie_function id) BUGLE_EXPORT_POST;

/* The BUGLE_ prefix is built up across several macros because using ##
 * inhibits macro expansion.
 */
#define _BUGLE_API_EXTENSION_ID(symbol, name) \
    _BUDGIE_ID_FULL(bugle_api_extension, bugle_api_extension_id, BUGLE ## symbol, name)
#define BUGLE_API_EXTENSION_ID(ext) \
    _BUGLE_API_EXTENSION_ID(_ ## ext, #ext)

#endif /* BUGLE_SRC_APIREFLECT_H */
