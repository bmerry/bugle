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
/* getaddrinfo only exists on XP and newer */
#ifndef WINVER
# define WINVER 0x0501
#endif
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <io.h>
#include <string.h>
#include <bugle/bool.h>
#include <bugle/io.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include "common/io-impl.h"
#include "platform/threads.h"
#include "platform/io.h"

/* Reader structure for a HANDLE-based pipe. It does not work for
 * sockets, because Winsock is just flat-out whacky and doesn't properly
 * integrate with HANDLE-based APIs.
 */
typedef struct 
{
    HANDLE handle;
    bugle_bool do_close; /* to allow a reader and a writer on a single pipe */
} bugle_io_reader_handle;

static size_t handle_read(void *ptr, size_t size, size_t nmemb, void *arg)
{
    bugle_io_reader_handle *s = (bugle_io_reader_handle *) arg;
    DWORD remain;
    DWORD received = 0;

    remain = size * nmemb; /* FIXME handle overflow */
    while (remain > 0)
    {
        DWORD bytes_read = 0;
        BOOL status;
        
        status = ReadFile(s->handle, (char *)ptr + received, remain, &bytes_read, NULL);
        received += bytes_read;
        remain -= bytes_read;
        if (!status)
            return received / size;
    }
    return nmemb;
}

static int handle_reader_close(void *arg)
{
    bugle_io_reader_handle *s = (bugle_io_reader_handle *) arg;
    int ret;

    if (!s->do_close)
        return 0;

    if (CloseHandle(s->handle))
        ret = 0;
    else
        ret = EOF;

    bugle_free(arg);
    return ret;
}

bugle_io_reader *bugle_io_reader_handle_new(HANDLE handle, bugle_bool do_close)
{
    bugle_io_reader *reader;
    bugle_io_reader_handle *s;

    reader = BUGLE_MALLOC(bugle_io_reader);
    s = BUGLE_MALLOC(bugle_io_reader_handle);

    reader->fn_read = handle_read;
    reader->fn_close = handle_reader_close;
    reader->arg = s;

    s->handle = handle;
    s->do_close = do_close;
    return reader;
}

bugle_io_reader *bugle_io_reader_fd_new(int fd)
{
    HANDLE handle = (HANDLE) _get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE)
        return NULL;
    else
        return bugle_io_reader_handle_new(handle, BUGLE_TRUE);
}

/* Reader structure for Winsock sockets. This uses non-blocking sockets with
 * a select() call to obtain synchronous operations, because Winsock seems
 * to hold a lock during blocking reads that prevents any data from being
 * transmitted in another thread.
 */
typedef struct
{
    SOCKET sock;
    bugle_bool do_close; /* to allow a reader and a writer on a single socket */
} bugle_io_reader_socket;

static size_t socket_read(void *ptr, size_t size, size_t nmemb, void *arg)
{
    bugle_io_reader_socket *s = (bugle_io_reader_socket *) arg;
    DWORD remain;
    DWORD received = 0;

    remain = size * nmemb; /* FIXME handle overflow */
    while (remain > 0)
    {
        DWORD bytes_read = 0;
        BOOL status = TRUE;
        int ret;
        fd_set read_fds;

        FD_ZERO(&read_fds);
        FD_SET(s->sock, &read_fds);
        select(s->sock + 1, &read_fds, NULL, NULL, NULL);
        ret = recv(s->sock, (char *)ptr + received, remain, 0);
        if (ret == SOCKET_ERROR)
        {
            /* Deal with spurious wakeups from select */
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                status = FALSE;
        }
        else
            bytes_read = ret;
        
        received += bytes_read;
        remain -= bytes_read;
        if (!status)
            return received / size;
    }
    return nmemb;
}

static int socket_reader_close(void *arg)
{
    bugle_io_reader_socket *s = (bugle_io_reader_socket *) arg;
    int ret;

    if (!s->do_close)
        return 0;

    if (closesocket(s->sock) == 0)
        ret = 0;
    else
        ret = EOF;

    bugle_free(arg);
    return ret;
}

bugle_io_reader *bugle_io_reader_socket_new(SOCKET sock, bugle_bool do_close)
{
    bugle_io_reader *reader;
    bugle_io_reader_socket *s;

    reader = BUGLE_MALLOC(bugle_io_reader);
    s = BUGLE_MALLOC(bugle_io_reader_socket);

    reader->fn_read = socket_read;
    reader->fn_close = socket_reader_close;
    reader->arg = s;

    s->sock = sock;
    s->do_close = do_close;
    return reader;
}

/* Writer structure for HANDLE-based pipes. Does not work with sockets
 * (see comments on bugle_io_reader_handle).
 */
typedef struct
{
    HANDLE handle;
    bugle_bool do_close;
} bugle_io_writer_handle;

static size_t handle_write(const void *ptr, size_t size, size_t nmemb, void *arg)
{
    bugle_io_writer_handle *s = (bugle_io_writer_handle *) arg;
    DWORD written = 0;
    DWORD remain;

    remain = size * nmemb;
    while (remain > 0)
    {
        DWORD bytes_written = 0;
        BOOL status;

        status = WriteFile(s->handle, (char *)ptr + written, remain, &bytes_written, NULL);

        written += bytes_written;
        remain -= bytes_written;
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

    if (CloseHandle(s->handle))
        ret = 0;
    else
        ret = EOF;

    bugle_free(arg);
    return ret;
}

bugle_io_writer *bugle_io_writer_handle_new(HANDLE handle, bugle_bool do_close)
{
    bugle_io_writer *writer;
    bugle_io_writer_handle *s;

    writer = BUGLE_MALLOC(bugle_io_writer);
    s = BUGLE_MALLOC(bugle_io_writer_handle);

    writer->fn_vprintf = NULL;
    writer->fn_putc = NULL;
    writer->fn_write = handle_write;
    writer->fn_close = handle_writer_close;
    writer->arg = s;

    s->handle = handle;
    s->do_close = do_close;
    return writer;
}

bugle_io_writer *bugle_io_writer_fd_new(int fd)
{
    HANDLE handle = (HANDLE) _get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE)
        return NULL;
    else
        return bugle_io_writer_handle_new(handle, BUGLE_TRUE);
}

/* Writer for winsock sockets.
 */
typedef struct
{
    SOCKET sock;
    bugle_bool do_close;
} bugle_io_writer_socket;

static size_t socket_write(const void *ptr, size_t size, size_t nmemb, void *arg)
{
    bugle_io_writer_socket *s = (bugle_io_writer_socket *) arg;
    DWORD written = 0;
    DWORD remain;

    remain = size * nmemb;
    while (remain > 0)
    {
        BOOL status = TRUE;
        DWORD bytes_written = 0;
        fd_set write_fds;
        int ret;

        FD_ZERO(&write_fds);
        FD_SET(s->sock, &write_fds);
        select(s->sock + 1, NULL, &write_fds, NULL, NULL);
        ret = send(s->sock, (char *)ptr + written, remain, 0);
        if (ret == SOCKET_ERROR)
        {
            /* Cater for spurious wakeups from select() */
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                status = FALSE;
        }
        else
        {
            bytes_written = ret;
        }

        written += bytes_written;
        remain -= bytes_written;
        if (!status)
            return written / size;
    }
    return nmemb;
}

static int socket_writer_close(void *arg)
{
    bugle_io_writer_socket *s = (bugle_io_writer_socket *) arg;
    int ret;

    if (!s->do_close)
        return 0;

    if (closesocket(s->sock) == 0)
        ret = 0;
    else
        ret = EOF;

    bugle_free(arg);
    return ret;
}

bugle_io_writer *bugle_io_writer_socket_new(SOCKET sock, bugle_bool do_close)
{
    bugle_io_writer *writer;
    bugle_io_writer_socket *s;

    writer = BUGLE_MALLOC(bugle_io_writer);
    s = BUGLE_MALLOC(bugle_io_writer_socket);

    writer->fn_vprintf = NULL;
    writer->fn_putc = NULL;
    writer->fn_write = socket_write;
    writer->fn_close = socket_writer_close;
    writer->arg = s;

    s->sock = sock;
    s->do_close = do_close;
    return writer;
}

static bugle_bool wsa_initialised = BUGLE_FALSE;

BUGLE_CONSTRUCTOR(wsa_initialise);
static void wsa_initialise(void)
{
    WSADATA wsa_data;
    int error;
    WORD version = MAKEWORD(2, 2);

    /* Request version 2.2 */
    error = WSAStartup(version, &wsa_data);
    if (error != 0)
        return;

    /* Winsock isn't required to give us what we asked for */
    if (wsa_data.wVersion != version)
    {
        WSACleanup();
        return;
    }

    wsa_initialised = BUGLE_TRUE;
}

char *bugle_io_socket_listen(const char *host, const char *port, bugle_io_reader **reader, bugle_io_writer **writer)
{
    int listen_sock;
    int sock;
    int status;
    struct addrinfo hints, *ai;

    BUGLE_RUN_CONSTRUCTOR(wsa_initialise);
    if (!wsa_initialised)
    {
        return "Could not initialise Winsock";
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;        /* supports IPv4 and IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    /* Mingw doesn't define AI_V4MAPPED or AI_ADDRCONFIG */
    hints.ai_flags = AI_PASSIVE;
#ifdef AI_V4MAPPED
    hints.ai_flags |= AI_V4MAPPED;
#endif
#ifdef AI_ADDRCONFIG
    hints.ai_flags |= AI_ADDRCONFIG;
#endif
    status = getaddrinfo(host, port, &hints, &ai);
    if (status != 0 || ai == NULL)
    {
        return bugle_asprintf("failed to resolve %s:%s: %s",
                              host ? host : "", port, gai_strerror(status));
    }


    /* Using FormatMessage to get error information out requires a
     * ridiculuous amount of jumping through hoops. For now, don't bother.
     * Also, we avoid the standard socket call because it creates an
     * "overlapped" socket that can't be used with synchronous ReadFile.
     */
    listen_sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (listen_sock == -1)
    {
        freeaddrinfo(ai);
        return bugle_asprintf("failed to open socket");
    }

    status = bind(listen_sock, ai->ai_addr, ai->ai_addrlen);
    if (status == -1)
    {
        freeaddrinfo(ai);
        return bugle_asprintf("failed to bind to %s:%s",
                              host ? host : "", port);
    }

    if (listen(listen_sock, 1) == -1)
    {
        freeaddrinfo(ai);
        closesocket(listen_sock);
        return bugle_asprintf("failed to listen on %s:%s",
                              host ? host : "", port);
    }

    sock = accept(listen_sock, NULL, NULL);
    if (sock == -1)
    {
        freeaddrinfo(ai);
        closesocket(listen_sock);
        return bugle_asprintf("failed to accept a connection on %s:%s",
                              host ? host : "", port);
    }
    freeaddrinfo(ai);
    closesocket(listen_sock);

    {
        u_long argp = 1;
        if (ioctlsocket(sock, FIONBIO, &argp) == SOCKET_ERROR)
        {
            closesocket(sock);
            return bugle_asprintf("failed to make socket non-blocking");
        }
    }

    /* Reader and writer can be closed independently. Ensure that the
     * socket doesn't get closed twice.
     */
    *reader = bugle_io_reader_socket_new(sock, BUGLE_TRUE);
    *writer = bugle_io_writer_socket_new(sock, BUGLE_FALSE);
    return NULL;
}
