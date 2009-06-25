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
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>
#include <bugle/string.h>
#include <bugle/io.h>
#include <bugle/memory.h>
#include "platform/io.h"
#include "common/io-impl.h"

#define BUFFER_SIZE 256

typedef struct bugle_io_reader_fd
{
    int fd;
} bugle_io_reader_fd;

static size_t fd_read(void *ptr, size_t size, size_t nmemb, void *arg)
{
    bugle_io_reader_fd *s;
    size_t remain;
    size_t received = 0;

    s = (bugle_io_reader_fd *) arg;
    remain = size * nmemb; /* FIXME handle overflow */
    while (remain > 0)
    {
        ssize_t cur;
        cur = read(s->fd, (char *)ptr + received, remain);
        if (cur < 0)
        {
            if (errno == EINTR)
                continue;
            else
                return received / size;
        }
        else
        {
            received += cur;
            remain -= cur;
        }
    }
    return nmemb;
}

static bugle_bool fd_has_data(void *arg)
{
    struct pollfd fds[1];
    bugle_io_reader_fd *s;

    s = (bugle_io_reader_fd *) arg;
    fds[0].fd = s->fd;
    fds[0].events = POLLIN;

    while (1)
    {
        int ret;
        ret = poll(fds, 1, 0);
        if (ret == -1)
        {
            if (errno != EINTR)
            {
                /* Fatal error */
                perror("poll");
                exit(1);
            }
        }
        else if (ret > 0 && (fds[0].revents & (POLLIN | POLLHUP)))
        {
            return BUGLE_TRUE;
        }
        else
        {
            /* Timeout i.e. no descriptors ready */
            return BUGLE_FALSE;
        }
    }
}

static int fd_reader_close(void *arg)
{
    bugle_io_reader_fd *s;

    s = (bugle_io_reader_fd *) arg;
    return close(s->fd) == 0 ? 0 : EOF;
}

bugle_io_reader *bugle_io_reader_fd_new(int fd)
{
    bugle_io_reader *reader;
    bugle_io_reader_fd *s;

    reader = BUGLE_MALLOC(bugle_io_reader);
    s = BUGLE_MALLOC(bugle_io_reader_fd);

    reader->fn_read = fd_read;
    reader->fn_has_data = fd_has_data;
    reader->fn_close = fd_reader_close;
    reader->arg = s;

    s->fd = fd;

    return reader;
}

bugle_io_reader *bugle_io_reader_socket_new(int sock)
{
    /* On POSIX, sockets are the same as file descriptors */
    return bugle_io_reader_fd_new(sock);
}

typedef struct bugle_io_writer_fd
{
    char buffer[BUFFER_SIZE];   /* Buffer for holding printf output */
    int fd;
} bugle_io_writer_fd;

static size_t fd_write(const void *ptr, size_t size, size_t nmemb, void *arg)
{
    bugle_io_writer_fd *s;
    size_t written = 0;
    ssize_t cur;
    size_t remain;

    /* FIXME: handle multiplication overflow */
    remain = size * nmemb;

    s = (bugle_io_writer_fd *) arg;
    while (remain > 0)
    {
        cur = write(s->fd, (char *)ptr + written, remain);
        if (cur < 0)
        {
            if (errno == EINTR)
                continue;
            else
                return written / size;
        }
        else
        {
            written += cur;
            remain -= cur;
        }
    }
    return nmemb;
}

static int fd_vprintf(void *arg, const char *format, va_list ap)
{
    bugle_io_writer_fd *s;
    int length;
    char *buffer;
    va_list ap2; /* backup copy for second pass */
    int ret;

#if HAVE_DECL_VA_COPY
    va_copy(ap2, ap);
#elif HAVE_DECL___VA_COPY
    __va_copy(ap2, ap);
#else
    memcpy(&ap2, &ap, sizeof(ap2));
#endif

    s = (bugle_io_writer_fd *) arg;
    length = bugle_vsnprintf(s->buffer, sizeof(s->buffer), format, ap);
    if (length < sizeof(s->buffer))
    {
        ret = fd_write(s->buffer, sizeof(char), length, arg);
    }
    else
    {
        buffer = bugle_vasprintf(format, ap);
        ret = fd_write(s->buffer, sizeof(char), length, arg);
        bugle_free(buffer);
    }
    va_end(ap2);
    return ret;
}

static int fd_putc(int c, void *arg)
{
    char ch;
    ch = c;
    return fd_write(&ch, 1, 1, arg);
}

static int fd_writer_close(void *arg)
{
    bugle_io_writer_fd *s;
    int ret;

    s = (bugle_io_writer_fd *) arg;
    ret = close(s->fd);
    if (ret == -1) return EOF;
    else return ret;
}

bugle_io_writer *bugle_io_writer_fd_new(int fd)
{
    bugle_io_writer_fd *s;
    bugle_io_writer *writer;

    writer = BUGLE_MALLOC(bugle_io_writer);
    s = BUGLE_MALLOC(bugle_io_writer_fd);

    writer->fn_vprintf = fd_vprintf;
    writer->fn_putc = fd_putc;
    writer->fn_write = fd_write;
    writer->fn_close = fd_writer_close;
    writer->arg = s;

    s->fd = fd;

    return writer;
}

bugle_io_writer *bugle_io_writer_socket_new(int sock)
{
    /* On POSIX, sockets are the same as file descriptors */
    return bugle_io_writer_fd_new(sock);
}
