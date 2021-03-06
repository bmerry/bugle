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

#ifndef BUGLE_GLX_GLXDUMP_H
#define BUGLE_GLX_GLXDUMP_H

#include <GL/glx.h>
#include <bugle/bool.h>
#include <stdio.h>
#include <budgie/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Count the attribute list for glXChooseVisual,
 * for which boolean attributes are not followed by a value.
 */
int bugle_count_glXChooseVisual_attributes(const int *attr);

/* Output the attribute list passed to glXChooseVisual */
bugle_bool bugle_dump_glXChooseVisual_attributes(const int *attr, bugle_io_writer *writer);

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_GLX_GLXDUMP_H */
