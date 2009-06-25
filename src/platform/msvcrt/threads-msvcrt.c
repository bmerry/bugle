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
#include "platform/threads.h"

void bugle_thread_once_win32(bugle_thread_once_t *once, void (*function)(void))
{
    if (InterlockedIncrement(&once->value) == 1)
    {
        /* We got here first. */
       (*function)();
       once->complete = 1; /* Mark completion */
    }
    else
    {
        /* Unlike with pthreads, there are no sync primitives that
         * can be statically initialised, so we have to spin. In the
         * bugle use cases, the chance of contention is low and the
         * initialiser functions should not be particularly long-running,
         * so we keep it simple and spin until full completion rather than
         * waiting for critical section to be created as in gnulib.
         */
        while (!once->complete)
        {
            Sleep(0);
        }
    }
}
