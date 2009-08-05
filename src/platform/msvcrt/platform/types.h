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

#ifdef __GNUC__
/* MinGW doesn't define the MSVC UINT8 etc types, but does provide C99 headers */
#include <inttypes.h>
typedef uint8_t  bugle_uint8_t;
typedef int8_t   bugle_int8_t;
typedef uint16_t bugle_uint16_t;
typedef int16_t  bugle_int16_t;
typedef uint32_t bugle_uint32_t;
typedef int32_t  bugle_int32_t;
typedef uint64_t bugle_uint64_t;
typedef int64_t  bugle_int64_t;

#define BUGLE_PRIu8 PRIu8
#define BUGLE_PRId8 PRId8
#define BUGLE_PRIu16 PRIu16
#define BUGLE_PRId16 PRId16
#define BUGLE_PRIu32 PRIu32
#define BUGLE_PRId32 PRId32
#define BUGLE_PRIu64 PRIu64
#define BUGLE_PRId64 PRId64

#else /* __GNUC__ */
typedef UINT8 bugle_uint8_t;
typedef INT8 bugle_int8_t;
typedef UINT16 bugle_uint16_t;
typedef INT16 bugle_int16_t;
typedef UINT32 bugle_uint32_t;
typedef INT32 bugle_int32_t;
typedef UINT64 bugle_uint64_t;
typedef INT64 bugle_int64_t;

#define BUGLE_PRIu8 "u"
#define BUGLE_PRId8 "d"
#define BUGLE_PRIu16 "hu"
#define BUGLE_PRId16 "hd"
#define BUGLE_PRIu32 "I32u"
#define BUGLE_PRId32 "I32d"
#define BUGLE_PRIu64 "I64u"
#define BUGLE_PRId64 "I64d"

#endif /* !__GNUC__ */

typedef LONG_PTR bugle_ssize_t;

typedef intptr_t bugle_pid_t;

#endif /* BUGLE_PLATFORM_TYPES_H */
