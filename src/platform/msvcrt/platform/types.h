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

#ifndef BUGLE_PLATFORM_TYPES_H
#define BUGLE_PLATFORM_TYPES_H

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>

typedef UINT8 bugle_uint8_t;
typedef INT8 bugle_int8_t;
typedef UINT16 bugle_uint16_t;
typedef INT16 bugle_int16_t;
typedef UINT32 bugle_uint32_t;
typedef INT32 bugle_int32_t;
typedef UINT64 bugle_uint64_t;
typedef INT64 bugle_int64_t;

typedef LONG_PTR bugle_ssize_t;

typedef intptr_t bugle_pid_t;

#define BUGLE_PRIu8 "u"
#define BUGLE_PRId8 "d"
#define BUGLE_PRIu16 "hu"
#define BUGLE_PRId16 "hd"
#define BUGLE_PRIu32 "I32u"
#define BUGLE_PRId32 "I32d"
#define BUGLE_PRIu64 "I64u"
#define BUGLE_PRId64 "I64d"

#endif /* BUGLE_PLATFORM_TYPES_H */
