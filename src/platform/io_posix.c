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
#ifdef _GNU_SOURCE
# undef _GNU_SOURCE /* It causes a GNU version of strerror_r to replace the POSIX version */
#endif
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <bugle/string.h>
#include <bugle/io.h>
#include <bugle/memory.h>
#include "platform/macros.h"
#include "common/io-impl.h"

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
    reader->fn_close = fd_reader_close;
    reader->arg = s;

    s->fd = fd;

    return reader;
}

typedef struct bugle_io_writer_fd
{
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

static int fd_writer_close(void *arg)
{
    bugle_io_writer_fd *s;
    int ret;

    s = (bugle_io_writer_fd *) arg;
    ret = close(s->fd);
    bugle_free(s);
    if (ret == -1) return EOF;
    else return ret;
}

bugle_io_writer *bugle_io_writer_fd_new(int fd)
{
    bugle_io_writer_fd *s;
    bugle_io_writer *writer;

    writer = BUGLE_MALLOC(bugle_io_writer);
    s = BUGLE_MALLOC(bugle_io_writer_fd);

    writer->fn_vprintf = NULL;
    writer->fn_putc = NULL;
    writer->fn_write = fd_write;
    writer->fn_close = fd_writer_close;
    writer->arg = s;

    s->fd = fd;

    return writer;
}

static int _bugle_strerror_r(int errnum, char *errbuf, size_t buflen)
{
#if _POSIX_THREAD_SAFE_FUNCTIONS > 0
    return strerror_r(errnum, errbuf, buflen);
#else
    const char *err;

    errno = 0;
    err = strerror(errnum);
    if (errno != 0)
        return errno;
    else
    {
        size_t len = strlen(err);
        if (len > buflen)
            return ERANGE;
        else
        {
            strcpy(errbuf, err);
            return 0;
        }
    }
#endif
}

char *bugle_io_socket_listen(const char *host, const char *port, bugle_io_reader **reader, bugle_io_writer **writer)
{
    int listen_sock;
    int sock, write_sock;
    int status;
    struct addrinfo hints, *ai;
    char err_buffer[1024];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;        /* supports IPv4 and IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG | AI_PASSIVE;
    status = getaddrinfo(host, port, &hints, &ai);
    if (status != 0 || ai == NULL)
    {
        return bugle_asprintf("failed to resolve %s:%s: %s",
                              host ? host : "", port, gai_strerror(status));
    }


    listen_sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (listen_sock == -1)
    {
        _bugle_strerror_r(errno, err_buffer, sizeof(err_buffer));
        freeaddrinfo(ai);
        return bugle_asprintf("failed to open socket: %s", err_buffer);
    }

    status = bind(listen_sock, ai->ai_addr, ai->ai_addrlen);
    if (status == -1)
    {
        _bugle_strerror_r(errno, err_buffer, sizeof(err_buffer));
        freeaddrinfo(ai);
        return bugle_asprintf("failed to bind to %s:%s: %s",
                              host ? host : "", port, err_buffer);
    }

    if (listen(listen_sock, 1) == -1)
    {
        _bugle_strerror_r(errno, err_buffer, sizeof(err_buffer));
        freeaddrinfo(ai);
        close(listen_sock);
        return bugle_asprintf("failed to listen on %s:%s: %s",
                              host ? host : "", port, err_buffer);
    }

    sock = accept(listen_sock, NULL, NULL);
    if (sock == -1)
    {
        _bugle_strerror_r(errno, err_buffer, sizeof(err_buffer));
        freeaddrinfo(ai);
        close(listen_sock);
        return bugle_asprintf("failed to accept a connection on %s:%s: %s",
                              host ? host : "", port, err_buffer);
    }
    freeaddrinfo(ai);
    close(listen_sock);

    /* reader and writer can be closed independently, so we need two handles
     * to the underlying socket
     */
    write_sock = dup(sock);
    if (write_sock == -1)
    {
        _bugle_strerror_r(errno, err_buffer, sizeof(err_buffer));
        close(sock);
        return bugle_asprintf("failed to duplicate socket: %s", err_buffer);
    }

    *reader = bugle_io_reader_fd_new(sock);
    *writer = bugle_io_writer_fd_new(write_sock);
    return NULL;
}
