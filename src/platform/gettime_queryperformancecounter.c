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

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stddef.h>
#include <bugle/time.h>

int bugle_gettime(bugle_timespec *tv)
{
    BOOL ret;
    LARGE_INTEGER count, freq;

    ret = QueryPerformanceCounter(&count);
    if (!ret) return -1;
    ret = QueryPerformanceFrequency(&freq);
    if (!ret) return -1;

    tv->tv_sec = count.QuadPart / freq.QuadPart;
    /* Remove part that is whole seconds */
    count.QuadPart -= tv->tv_sec * freq.QuadPart;
    /* If frequency > 9GHz, we can't do the obvious without
     * overflowing an int64, so round off a bit instead.
     */
    while (freq.QuadPart >= 9223372036LL)
    {
        freq.QuadPart >>= 1;
        count.QuadPart >>= 1;
    }
    tv->tv_nsec = count.QuadPart * 1000000000 / freq.QuadPart;
    return 0;
}
