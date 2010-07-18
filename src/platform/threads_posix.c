/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2010  Bruce Merry
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
#include <pthread.h>
#include <bugle/memory.h>
#include <stddef.h>
#include <errno.h>
#include "platform/threads.h"

int bugle_thread_sem_trywait(bugle_thread_sem_t *sem)
{
    int result = sem_trywait(sem);
    if (result == 0)
        return 0;
    else if (errno == EAGAIN)
        return 1;
    else
        return -1;
}

typedef struct
{
    unsigned int (*start)(void *);
    void *arg;
    unsigned int ret;
} bugle_thread_wrapper;

static void *bugle_thread_spawn(void *arg)
{
    bugle_thread_wrapper *wrapper = (bugle_thread_wrapper *) arg;
    wrapper->ret = wrapper->start(wrapper->arg);
    return arg;
}

int bugle_thread_create(bugle_thread_handle *thread, unsigned int (*start)(void *), void *arg)
{
    bugle_thread_wrapper *wrapper = BUGLE_MALLOC(bugle_thread_wrapper);
    wrapper->start = start;
    wrapper->arg = arg;
    wrapper->ret = 0;

    int status = pthread_create(thread, NULL, bugle_thread_spawn, wrapper);
    if (status == 0)
        return 0;
    else
    {
        bugle_free(wrapper);
        return status;
    }
}

int bugle_thread_join(bugle_thread_handle thread, unsigned int *retval)
{
    bugle_thread_wrapper *wrapper;

    int status = pthread_join(thread, (void **) &wrapper);
    if (status != 0)
        return status;
    else
    {
        if (retval != NULL) *retval = wrapper->ret;
        bugle_free(wrapper);
        return 0;
    }
}
