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

#ifndef BUGLE_GL_GLEXTENSIONS_H
#define BUGLE_GL_GLEXTENSIONS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>
#include <bugle/apireflect.h>
#include <budgie/macros.h>

bool bugle_gl_has_extension(bugle_api_extension ext);
bool bugle_gl_has_extension_group(bugle_api_extension ext);
/* More robust versions: if ext == -1, checks for the string in a hash table) */
bool bugle_gl_has_extension2(bugle_api_extension ext, const char *name);
bool bugle_gl_has_extension_group2(bugle_api_extension ext, const char *name);

/* The BUGLE_ prefix is built up across several macros because using ##
 * inhibits macro expansion.
 */
#define BUGLE_GL_HAS_EXTENSION(ext) \
    (bugle_gl_has_extension2(_BUGLE_API_EXTENSION_ID(_ ## ext, #ext), #ext))
#define BUGLE_GL_HAS_EXTENSION_GROUP(ext) \
    (bugle_gl_has_extension_group2(_BUGLE_API_EXTENSION_ID(_ ## ext, #ext), #ext))

/* Used by the initialisation code */
void glextensions_initialise(void);

#endif /* !BUGLE_GL_GLEXTENSIONS_H */
