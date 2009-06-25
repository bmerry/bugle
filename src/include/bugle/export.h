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

#ifndef BUGLE_EXPORT_H
#define BUGLE_EXPORT_H

/* To declare a function that should have cross-DSO visibility, declare it
 * as
 *
 * BUGLE_EXPORT_PRE void bugle_function(int arg) BUGLE_EXPORT_POST;
 */

#if defined(__GNUC__)

#define BUGLE_EXPORT_PRE
#define BUGLE_EXPORT_POST __attribute__((visibility("default")))

#elif defined(_MSC_VER)

#define BUGLE_EXPORT_PRE __declspec(dllexport)
#define BUGLE_EXPORT_POST

#else

#define BUGLE_EXPORT_PRE
#define BUGLE_EXPORT_POST

#endif

#endif /* !BUGLE_EXPORT_H */
