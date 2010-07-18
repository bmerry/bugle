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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "platform_config.h"
#include <stddef.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <bugle/time.h>

#if _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK)

int bugle_gettime(bugle_timespec *ts)
{
    struct timespec t;
    int ret;

    ret = clock_gettime(CLOCK_MONOTONIC, &t);
    if (ret != 0) return ret;
    ts->tv_sec = t.tv_sec;
    ts->tv_nsec = t.tv_nsec;
    return 0;
}

#else

int bugle_gettime(bugle_timespec *ts)
{
    struct timeval t;
    int ret;

    /* FIXME: use clock_gettime with CLOCK_MONOTONIC if available */
    ret = gettimeofday(&t, NULL);
    if (ret != 0) return ret;
    ts->tv_sec = t.tv_sec;
    ts->tv_nsec = t.tv_usec * 1000;
    return 0;
}

#endif
