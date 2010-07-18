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

#ifndef BUGLE_TIME_H
#define BUGLE_TIME_H

#include <time.h>
#include <bugle/export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bugle_timespec_s
{
    time_t tv_sec;
    long tv_nsec;
} bugle_timespec;

/* Returns a timestamp, relative to an arbitrary origin. Should only be
 * used for relative time calculations.
 */
BUGLE_EXPORT_PRE int bugle_gettime(bugle_timespec *tv) BUGLE_EXPORT_POST;

#endif /* !BUGLE_TIME_H */
