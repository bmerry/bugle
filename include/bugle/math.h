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

/* Non-portable mathematical utilities */

#ifndef BUGLE_MATH_H
#define BUGLE_MATH_H

#include <bugle/export.h>

/* Equivalent to C99 functions, but specific to double */
BUGLE_EXPORT_PRE int bugle_isfinite(double x) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int bugle_isnan(double x) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE double bugle_nan(void) BUGLE_EXPORT_POST;

/* Rounds to nearest, but halfway cases are not defined */
BUGLE_EXPORT_PRE double bugle_round(double x) BUGLE_EXPORT_POST;

#endif /* BUGLE_MATH_H */
