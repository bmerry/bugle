/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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

/* Implements support for the ability to tell whether an FD has data available
 * on Win32, using a separate thread to continually read data into a buffer.
 * See protocol.c for interface documentation of these functions.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/porting.h>

#if BUGLE_OSAPI_WIN32

#define WIN32_LEAN_AND_MEAN
#include <bugle/bool.h>
#include <fcntl.h>
#include <errno.h>
#include "lock.h"
#include "xalloc.h"
#include <windows.h>
#include <process.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096

typedef struct gldb_protocol_reader_select
{
    /* the underlying fd to read */
    int fd;

    /* Indices into a circular buffer. read_idx is where the main thread gets
     * data. write_idx is where the thread puts new data from the fd.
     * read_idx == write_idx means the buffer is empty.
     */
    int read_idx, write_idx; 

    /* Set to BUGLE_TRUE by the thread to signal that no more data is coming. */
    bugle_bool is_eof;

    /* The circular buffer */
    char buffer[BUFFER_SIZE];

    /* Level-triggered events. data_event is set to indicate that the buffer is
     * non-empty, space_event is set to indicate that the buffer is not full.
     */
    HANDLE data_event, space_event;

    /* Mutex protecting the entire process. Generally the thread holds it,
     * except while performing a blocking operation (waiting for space or
     * reading data).
     */
    CRITICAL_SECTION lock;
    HANDLE thread;
} gldb_protocol_reader_select;

unsigned __stdcall reader_thread(void *arg)
{
    gldb_protocol_reader_select *r;

    r = (gldb_protocol_reader_select *) arg;
    EnterCriticalSection(&r->lock);
    while (BUGLE_TRUE)
    {
        ssize_t space;
        ssize_t bytes;

        /* Always leave one empty slot, otherwise full and empty look the same */
        if (r->write_idx >= r->read_idx)
        {
            space = BUFFER_SIZE - r->write_idx;
            if (r->read_idx == 0)
                space--;   /* don't completely fill */
        }
        else
            space = r->read_idx - r->write_idx - 1;
        if (space <= 0)
        {
            /* No space to read into */
            ResetEvent(r->space_event);
            LeaveCriticalSection(&r->lock);
            WaitForSingleObject(r->space_event, INFINITE);
            EnterCriticalSection(&r->lock);
            continue;  /* Go round and recompute space */
        }

        LeaveCriticalSection(&r->lock);
        bytes = _read(r->fd, r->buffer + r->write_idx, space);
        EnterCriticalSection(&r->lock);

        if (bytes <= 0)
        {
            r->is_eof = BUGLE_TRUE;
            SetEvent(r->data_event);
            LeaveCriticalSection(&r->lock);
            return 0;
        }
        else
        {
            r->write_idx += bytes;
            if (r->write_idx >= BUFFER_SIZE)
                r->write_idx -= BUFFER_SIZE;
            SetEvent(r->data_event);
        }
    }
    return 0;
}

gldb_protocol_reader_select *gldb_protocol_reader_select_new(int fd)
{
    gldb_protocol_reader_select *r;

    r = XMALLOC(gldb_protocol_reader_select);
    InitializeCriticalSection(&r->lock);
    r->data_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    r->space_event = CreateEvent(NULL, TRUE, TRUE, NULL);
    r->fd = fd;
    r->is_eof = BUGLE_FALSE;
    r->read_idx = 0;
    r->write_idx = 0;
    EnterCriticalSection(&r->lock);
    r->thread = (HANDLE) _beginthreadex(NULL, 0, reader_thread, r, 0, NULL);
    LeaveCriticalSection(&r->lock);
    return r;
}

ssize_t gldb_protocol_reader_select_read(gldb_protocol_reader_select *r, void *buf, size_t count)
{
    size_t ret = 0;
    size_t copied;
    char *data;

    data = (char *) buf;  /* to simplify pointer arithmetic */
    EnterCriticalSection(&r->lock);
    while (count > 0)
    {
        if (r->read_idx == r->write_idx)
        {
            if (r->is_eof)
                break;
            ResetEvent(r->data_event);
            LeaveCriticalSection(&r->lock);
            WaitForSingleObject(r->data_event, INFINITE);
            EnterCriticalSection(&r->lock);
            continue;
        }

        if (r->read_idx > r->write_idx)
            copied = BUFFER_SIZE - r->read_idx;
        else
            copied = r->write_idx - r->read_idx;

        if (count < copied)
            copied = count;
        memcpy(data, r->buffer + r->read_idx, copied);
        data += copied;
        ret += copied;
        count -= copied;
        r->read_idx += copied;
        if (r->read_idx >= BUFFER_SIZE)
            r->read_idx -= BUFFER_SIZE;
        SetEvent(r->space_event);
    }
    LeaveCriticalSection(&r->lock);
    return ret;
}

bugle_bool gldb_protocol_reader_select_has_data(gldb_protocol_reader_select *r)
{
    bugle_bool ret;

    EnterCriticalSection(&r->lock);
    ret = (r->read_idx != r->write_idx);
    LeaveCriticalSection(&r->lock);
    return ret;
}

void gldb_protocol_reader_select_free(gldb_protocol_reader_select *r)
{
    EnterCriticalSection(&r->lock);

    /* Since we have the mutex, the thread must be already exited or in a
     * blocking call (or just either side of one). It is thus relatively
     * harmless to terminate it. It is not possible to signal it to terminate
     * because we have no way to interrupt _read().
     */
    TerminateThread(r->thread, 0);
    WaitForSingleObject(r->thread, INFINITE);  /* wait for thread termination */
    CloseHandle(r->thread);
    free(r);
}

#endif /* BUGLE_OSAPI_WIN32 */
