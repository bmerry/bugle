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

#ifndef BUGLE_PLATFORM_MACROS_H
#define BUGLE_PLATFORM_MACROS_H

#include <stdarg.h>

/* GCC bug prevents va_copy from being defined when using -std=c89.
 * However, it will define __va_copy instead.
 */
#if defined(__va_copy) && !defined(va_copy)
# define BUGLE_VA_COPY(trg, src) __va_copy(trg, src)
#else
# define BUGLE_VA_COPY(trg, src) va_copy(trg, src)
#endif

#endif /* BUGLE_PLATFORM_MACROS_H */
