/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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

/* Dumping code that is function-agnostic, and does not depend on being
 * able to call GLX (hence suitable for incorporating into gldb).
 */

#ifndef BUGLE_GLX_GLXTYPES_H
#define BUGLE_GLX_GLXTYPES_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glx.h>
#include <stdio.h>
#include <stdbool.h>
#include <bugle/glx/overrides.h>

bool bugle_dump_Bool(Bool b, FILE *out);
bool bugle_dump_GLXDrawable(GLXDrawable d, FILE *out);

#endif /* !BUGLE_GLX_GLXTYPES_H */
