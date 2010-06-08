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

/* Still TODO:
 * - Implement bugle_io_socket_listen
 * - handle_has_data won't currently work for pipes between the debugger and
 *   the debug filter. Probably the best option is just to put in threading
 *   at the debug filter level, and have one thread run a read pump that
 *   passes messages to the debug thread via a semaphore. An alternative is
 *   to actually read some data via async I/O and buffer it for reads, and
 *   check the status of the async I/O to tell if there is data ready.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "platform_config.h"
#ifndef _WIN32_LEAN_AND_MEAN
# define _WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <time.h>
#include <io.h>

enum bugle_io_handle_type
{
    BUGLE_HANDLE_TYPE_SOCKET,
    BUGLE_HANDLE_TYPE_FILE
};

typedef struct bugle_io_reader_handle
{
    HANDLE handle;
    bugle_io_handle_type type;
    bugle_bool do_close; /* to allow a reader and a writer on a single socket */
} bugle_io_reader_handle;

static size_t handle_read(void *ptr, size_t size, size_t nmemb, void *arg)
{
    bugle_io_reader_handle *s = (bugle_io_reader_handle *) arg;
    DWORD remain;
    DWORD received = 0;

    remain = size * nmemb; /* FIXME handle overflow */
    while (remain > 0)
    {
        DWORD bytes_read;
        BOOL status = ReadFile(s->handle, (char *)ptr + received, remain, &bytes_read, NULL);
        received += bytes_read;
        remain -= bytes_read;
        if (!status)
            return received / size;
    }
    return nmemb;
}

static bugle_bool handle_has_data(void *arg)
{
    /* This only works for sockets at present - I don't know how to tell
     * whether other I/O handles have data
     */
    bugle_io_reader_handle *s = (bugle_io_reader_handle *) arg;

    switch (s->type)
    {
    case BUGLE_HANDLE_TYPE_FILE:
        return bugle_true;
    case BUGLE_HANDLE_TYPE_SOCKET:
        {
            fd_set readfds;
            int status;
            const struct timeval timeout = {0, 0};

            FD_ZERO(readfds);
            FD_SET(s->handle, readfds);
            status = select(1, &readfds, NULL, NULL, &timeout);
            if (status > 0)
                return BUGLE_TRUE;  /* data available now */
            else if (status == 0)
                return BUGLE_FALSE; /* no data available, timed out */
            else
                /* No way to tell, take a guess */
                return BUGLE_FALSE;
        }
    }
    return BUGLE_FALSE; /* Should never be reached */
}

static int handle_reader_close(void *arg)
{
    bugle_io_reader_handle *s = (bugle_io_reader_handle *) arg;
    int ret;

    if (!s->do_close)
        return 0;

    switch (s->type)
    {
    case BUGLE_HANDLE_TYPE_SOCKET:
        if (closesocket(s->handle) == 0)
            ret = 0;
        else
            ret = EOF;
        break;
    default:
        if (CloseHandle(s->handle))
            ret = 0;
        else
            ret = EOF;
        break;
    }
}

static bugle_io_reader *bugle_io_reader_handle_new(HANDLE handle, bugle_io_handle_type type, bugle_bool do_close)
{
    bugle_io_reader *reader;
    bugle_io_reader_handle *s;

    reader = BUGLE_MALLOC(bugle_io_reader);
    s = BUGLE_MALLOC(bugle_io_reader_handle);

    reader->fn_read = handle_read;
    reader->fn_has_data = handle_has_data;
    reader->fn_close = handle_reader_close;
    reader->arg = s;

    s->handle = handle;
    s->type = type;
    s->do_close = do_close;
    return reader;
}

bugle_io_reader *bugle_io_reader_fd_new(HANDLE fd)
{
    return bugle_io_reader_handle_new(fd, BUGLE_HANDLE_TYPE_FILE, BUGLE_TRUE);
}

bugle_io_reader *bugle_io_reader_socket_new(SOCKET fd)
{
    return bugle_io_reader_handle_new(fd, BUGLE_HANDLE_TYPE_SOCKET, BUGLE_TRUE);
}

typedef struct bugle_io_writer_handle
{
    HANDLE handle;
    bugle_io_handle_type type;
    bugle_bool do_close;
} bugle_io_writer_handle;

static size_t handle_write(const void *ptr, size_t size, size_t nmemb, void *arg)
{
    bugle_io_writer_handle *s = (bugle_io_writer_handle *) arg;
    DWORD written = 0;
    DWORD remain;
    DWORD cur;

    /* FIXME: handle overflow */
    remain = size * nmemb;
    while (remain > 0)
    {
        BOOL status = WriteFile(s->handle, (char *)ptr + written, remain, &cur, NULL);
        written += cur;
        remain -= cur;
        if (!status)
            return written / size;
    }
    return nmemb;
}

static int handle_writer_close(void *arg)
{
    bugle_io_writer_handle *s = (bugle_io_writer_handle *) arg;
    int ret;

    if (!s->do_close)
        return 0;

    switch (s->type)
    {
    case BUGLE_HANDLE_TYPE_SOCKET:
        if (closesocket(s->handle) == 0)
            ret = 0;
        else
            ret = EOF;
        break;
    default:
        if (CloseHandle(s->handle))
            ret = 0;
        else
            ret = EOF;
        break;
    }
}

static bugle_io_writer *bugle_io_writer_handle_new(HANDLE handle, bugle_io_handle_type type, bugle_bool do_close)
{
    bugle_io_writer *writer;
    bugle_io_writer_handle *s;

    writer = BUGLE_MALLOC(bugle_io_writer);
    s = BUGLE_MALLOC(bugle_io_writer_handle);

    writer->fn_vprintf = NULL;
    writer->fn_putc = NULL;
    writer->fn_write = handle_write;
    reader->fn_close = handle_writer_close;
    reader->arg = s;

    s->handle = handle;
    s->type = type;
    s->do_close = do_close;
    return writer;
}

bugle_io_writer *bugle_io_writer_fd_new(int fd)
{
    HANDLE handle = _get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE)
        return NULL;
    else
        return bugle_io_writer_handle_new(handle, BUGLE_HANDLE_TYPE_FILE, BUGLE_TRUE);
}

static bugle_io_writer *bugle_io_writer_socket_new(SOCKET sock)
{
    return bugle_io_writer_handle_new(sock, BUGLE_HANDLE_TYPE_SOCKET, BUGLE_TRUE);
}

char *bugle_io_socket_listen(const char *host, const char *port, bugle_io_reader **reader, bugle_io_writer **writer)
{
    /* TODO */
}
