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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "full-read.h"
#include "full-write.h"
#include "lock.h"
#include "xalloc.h"
#include "protocol.h"
#include <bugle/misc.h>
#include <bugle/porting.h>

/* Trying to detect whether or not there is a working htonl, what header
 * it is in, and what library to link against is far more effort than
 * simply writing it from scratch.
 */
static uint32_t io_htonl(uint32_t h)
{
    union
    {
        uint8_t bytes[4];
        uint32_t n;
    } u;

    u.bytes[0] = (h >> 24) & 0xff;
    u.bytes[1] = (h >> 16) & 0xff;
    u.bytes[2] = (h >> 8) & 0xff;
    u.bytes[3] = h & 0xff;
    return u.n;
}

static uint32_t io_ntohl(uint32_t n)
{
    union
    {
        uint8_t bytes[4];
        uint32_t n;
    } u;

    u.n = n;
    return (u.bytes[0] << 24) | (u.bytes[1] << 16) | (u.bytes[2] << 8) | u.bytes[3];
}

#define TO_NETWORK(x) io_htonl(x)
#define TO_HOST(x) io_ntohl(x)

#if BUGLE_OSAPI_SELECT_BROKEN
# define IO_BUFFER_SIZE 4096

static int io_buffered_fd = -1;
static char io_buffer[IO_BUFFER_SIZE];
static int io_read_idx;  /* where the user finds data */
static int io_write_idx; /* where the helper thread writes new data */
static bool io_eof;      /* set to true once the helper runs out of input */

#if BUGLE_OSAPI_WIN32
# include <windows.h>
# include <process.h>
# define IO_THREAD_CALL __stdcall
# define IO_THREAD_RETURN_TYPE unsigned
# define IO_THREAD_RETURN 0U

static HANDLE io_data_event, io_space_event;
gl_lock_define(static, io_mutex)

static IO_THREAD_RETURN_TYPE IO_THREAD_CALL io_read_thread(void *arg);

static void io_lock(int fd)
{
    gl_lock_lock(io_mutex);
}

static void io_unlock(int fd)
{
    gl_lock_unlock(io_mutex);
}

static void io_mark_data(int fd)
{
    SetEvent(io_data_event);
}

static void io_mark_no_data(int fd)
{
    ResetEvent(io_data_event);
}

static void io_mark_space(int fd)
{
    SetEvent(io_space_event);
}

static void io_mark_no_space(int fd)
{
    ResetEvent(io_space_event);
}

static void io_wait_space(int fd)
{
    WaitForSingleObject(io_space_event, INFINITE);
}

static void io_wait_data(int fd)
{
    WaitForSingleObject(io_data_event, INFINITE);
}

void gldb_io_start_read_thread(int fd)
{
    io_lock(fd);
    assert(io_buffered_fd == -1);
    io_buffered_fd = fd;

    io_data_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    io_space_event = CreateEvent(NULL, TRUE, TRUE, NULL);
    _beginthreadex(NULL, 0, io_read_thread, &io_buffered_fd, 0, NULL);

    io_unlock(fd);
}

#else
# error "Please add code to protocol.c to handle non-blocking I/O"
#endif
static IO_THREAD_RETURN_TYPE IO_THREAD_CALL io_read_thread(void *arg)
{
    int fd;

    fd = *(int *) arg;
    io_lock(fd);
    while (true)
    {
        int space;
        ssize_t bytes;

        /* Always leave one empty slot, otherwise full and empty look the same */
        if (io_write_idx >= io_read_idx)
        {
            space = IO_BUFFER_SIZE - io_write_idx;
            if (io_read_idx == 0)
                space--;   /* don't completely fill */
        }
        else
            space = io_read_idx - io_write_idx - 1;
        if (space <= 0)
        {
            /* No space to read into */
            io_mark_no_space(fd);
            io_unlock(fd);
            io_wait_space(fd);
            io_lock(fd);
            continue;  /* Go round and recompute space */
        }

        io_unlock(fd);
        bytes = read(fd, io_buffer + io_write_idx, space);
        io_lock(fd);

        if (bytes <= 0)
        {
            if (bytes < 0 && errno == EINTR)
                continue;
            io_eof = true;
            io_mark_data(fd); /* No actual data, but unblocks main thread */
            io_unlock(fd);
            return IO_THREAD_RETURN;
        }
        else
        {
            io_write_idx += bytes;
            if (io_write_idx >= IO_BUFFER_SIZE)
                io_write_idx -= IO_BUFFER_SIZE;
            io_mark_data(fd);
        }
    }
    return IO_THREAD_RETURN;
}

static ssize_t io_read(int fd, char *data, size_t count)
{
    size_t ret = 0;
    size_t copied;

    io_lock(fd);
    if (fd != io_buffered_fd)
    {
        io_unlock(fd);
        return full_read(fd, data, count);
    }

    while (count > 0)
    {
        if (io_read_idx == io_write_idx)
        {
            if (io_eof)
                break;
            io_mark_no_data(fd);
            io_unlock(fd);
            io_wait_data(fd);
            io_lock(fd);
            continue;
        }

        if (io_read_idx > io_write_idx)
            copied = IO_BUFFER_SIZE - io_read_idx;
        else
            copied = io_write_idx - io_read_idx;

        if (count < copied)
            copied = count;
        memcpy(data, io_buffer + io_read_idx, copied);
        data += copied;
        ret += copied;
        count -= copied;
        io_read_idx += copied;
        if (io_read_idx >= IO_BUFFER_SIZE)
            io_read_idx -= IO_BUFFER_SIZE;
        io_mark_space(fd);
    }
    return ret;
}

bool gldb_io_has_data(int fd)
{
    bool ret = false;

    io_lock(fd);
    if (fd == io_buffered_fd)
    {
        ret = (io_read_idx != io_write_idx) || io_eof;
    }
    io_unlock(fd);
    return ret;
}

#else /* !BUGLE_OSAPI_SELECT_BROKEN */

static ssize_t io_read(int fd, char *data, size_t count)
{
    return full_read(fd, data, count);
}

static ssize_t gldb_io_has_data(int fd)
{
    fd_set read_fds;
    struct timeval timeout;
    int r;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    r = select(in_pipe + 1, &readfds, NULL, &exceptfds, &timeout);
    if (r == -1)
    {
        if (errno == EINTR) continue;
        else
        {
            perror("select");
            exit(1);
        }
    }
    return FD_ISSET(in, &readfds);
}

void gldb_io_start_read_thread(int fd)
{
    /* Nop */
}

#endif

/* Returns false on EOF, aborts on failure */
static bool io_safe_write(int fd, const void *buf, size_t count)
{
    ssize_t out;

    out = full_write(fd, buf, count);
    if (out < count)
    {
        if (errno)
        {
            perror("write failed");
            exit(1);
        }
        return false;
    }
    else
        return true;
}

/* Returns false on EOF, aborts on failure */
static bool io_safe_read(int fd, void *buf, size_t count)
{
    ssize_t out;

    out = io_read(fd, buf, count);
    if (out < count)
    {
        if (errno)
        {
            perror("read failed");
            exit(1);
        }
        return false;
    }
    else
        return true;
}

bool gldb_protocol_send_code(int fd, uint32_t code)
{
    uint32_t code2;

    code2 = TO_NETWORK(code);
    return io_safe_write(fd, &code2, sizeof(uint32_t));
}

bool gldb_protocol_send_binary_string(int fd, uint32_t len, const char *str)
{
    uint32_t len2;

    /* FIXME: on 64-bit systems, length could in theory be >=2^32. This is
     * not as unlikely as it sounds, since textures are retrieved in floating
     * point and so might be several times larger on the wire than on the
     * video card. If this happens, the logical thing is to make a packet size
     * of exactly 2^32-1 signal that a continuation packet will follow, which
     * avoid breaking backwards compatibility. Alternatively, it might be
     * better to just break backwards compatibility.
     */
    len2 = TO_NETWORK(len);
    if (!io_safe_write(fd, &len2, sizeof(uint32_t))) return false;
    if (!io_safe_write(fd, str, len)) return false;
    return true;
}

bool gldb_protocol_send_string(int fd, const char *str)
{
    return gldb_protocol_send_binary_string(fd, strlen(str), str);
}

bool gldb_protocol_recv_code(int fd, uint32_t *code)
{
    uint32_t code2;
    if (io_safe_read(fd, &code2, sizeof(uint32_t)))
    {
        *code = TO_HOST(code2);
        return true;
    }
    else
        return false;
}

bool gldb_protocol_recv_binary_string(int fd, uint32_t *len, char **data)
{
    uint32_t len2;
    int old_errno;

    if (!io_safe_read(fd, &len2, sizeof(uint32_t))) return false;
    *len = TO_HOST(len2);
    *data = xmalloc(*len + 1);
    if (!io_safe_read(fd, *data, *len))
    {
        old_errno = errno;
        free(*data);
        errno = old_errno;
        return false;
    }
    else
    {
        (*data)[*len] = '\0';
        return true;
    }
}

bool gldb_protocol_recv_string(int fd, char **str)
{
    uint32_t dummy;

    return gldb_protocol_recv_binary_string(fd, &dummy, str);
}
