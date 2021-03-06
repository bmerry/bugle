/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009, 2013  Bruce Merry
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

#ifndef BUGLE_PLATFORM_TYPES_H
#define BUGLE_PLATFORM_TYPES_H

typedef unsigned char bugle_uint8_t;
typedef signed char bugle_int8_t;
typedef unsigned short bugle_uint16_t;
typedef signed short bugle_int16_t;
typedef unsigned int bugle_uint32_t;
typedef signed int bugle_int32_t;
typedef unsigned long bugle_uint64_t;
typedef signed long bugle_int64_t;
typedef unsigned long bugle_uint64_t;
typedef unsigned long bugle_uintptr_t;

typedef unsigned long bugle_ssize_t;

typedef unsigned int bugle_pid_t;

#define BUGLE_PRIu8 "u"
#define BUGLE_PRId8 "d"
#define BUGLE_PRIu16 "u"
#define BUGLE_PRId16 "d"
#define BUGLE_PRIu32 "u"
#define BUGLE_PRId32 "d"
#define BUGLE_PRIu64 "u"
#define BUGLE_PRId64 "d"
#define BUGLE_PRIxPTR "x"

#endif /* BUGLE_PLATFORM_TYPES_H */
