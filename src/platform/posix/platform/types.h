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

#include "platform_config.h"
#include <inttypes.h>
#include <sys/types.h>

typedef uint8_t bugle_uint8_t;
typedef int8_t bugle_int8_t;
typedef uint16_t bugle_uint16_t;
typedef int16_t bugle_int16_t;
typedef uint32_t bugle_uint32_t;
typedef int32_t bugle_int32_t;
typedef uint64_t bugle_uint64_t;
typedef int64_t bugle_int64_t;
typedef uintptr_t bugle_uintptr_t;

typedef ssize_t bugle_ssize_t;

typedef pid_t bugle_pid_t;

#define BUGLE_PRIu8 PRIu8
#define BUGLE_PRId8 PRId8
#define BUGLE_PRIu16 PRIu16
#define BUGLE_PRId16 PRId16
#define BUGLE_PRIu32 PRIu32
#define BUGLE_PRId32 PRId32
#define BUGLE_PRIu64 PRIu64
#define BUGLE_PRId64 PRId64
#define BUGLE_PRIxPTR PRIxPTR

#endif /* BUGLE_PLATFORM_TYPES_H */
