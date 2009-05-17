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
#include <bugle/bool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include "full-read.h"
#include "full-write.h"
#include "protocol.h"
#include <bugle/misc.h>
#include <bugle/memory.h>
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

/* These functions are all defined in an OS-specific file e.g. protocol-win32.c */

typedef struct gldb_protocol_reader_select gldb_protocol_reader_select;

/* Create a new OS-specific reader object for the given fd */
gldb_protocol_reader_select *gldb_protocol_reader_select_new(int fd);

/* Equivalent read() for the reader object. Not guaranteed to extract all
 * currently available data.
 */
ssize_t gldb_protocol_reader_select_read(gldb_protocol_reader_select *r, void *buf, size_t count);

/* Determine whether there is some data available to read */
bugle_bool gldb_protocol_reader_select_has_data(gldb_protocol_reader_select *r);

/* Clean up the structure. Does not close the fd. */
void gldb_protocol_reader_select_free(gldb_protocol_reader_select *r);

#endif /* BUGLE_OSAPI_SELECT_BROKEN */

typedef enum
{
    MODE_FD,
    MODE_FD_SELECT,
    MODE_FUNC
} gldb_protocol_reader_mode;

struct gldb_protocol_reader
{
    gldb_protocol_reader_mode mode;
    int fd;
#if BUGLE_OSAPI_SELECT_BROKEN
    gldb_protocol_reader_select *select_reader;
#endif
    
    ssize_t (*reader_func)(void *arg, void *buf, size_t count);
    void *reader_arg;
};

gldb_protocol_reader *gldb_protocol_reader_new_fd(int fd)
{
    gldb_protocol_reader *reader;
    reader = BUGLE_MALLOC(gldb_protocol_reader);
    reader->mode = MODE_FD;
    reader->fd = fd;
    reader->reader_func = NULL;
    reader->reader_arg = NULL;
    return reader;
}

gldb_protocol_reader *gldb_protocol_reader_new_fd_select(int fd)
{
    gldb_protocol_reader *reader;
    reader = BUGLE_MALLOC(gldb_protocol_reader);
    reader->mode = MODE_FD_SELECT;
    reader->fd = fd;
    reader->reader_func = NULL;
    reader->reader_arg = NULL;
#if BUGLE_OSAPI_SELECT_BROKEN
    reader->select_reader = gldb_protocol_reader_select_new(fd);
#endif
    return reader;
}

gldb_protocol_reader *gldb_protocol_reader_new_func(ssize_t (*read_func)(void *arg, void *buf, size_t count), void *arg)
{
    gldb_protocol_reader *reader;
    reader = BUGLE_MALLOC(gldb_protocol_reader);
    reader->mode = MODE_FUNC;
    reader->fd = -1;
    reader->reader_func = read_func;
    reader->reader_arg = arg;
    return reader;
}

ssize_t gldb_protocol_reader_read(gldb_protocol_reader *reader, void *buf, size_t count)
{
    switch (reader->mode)
    {
    case MODE_FD:
        return read(reader->fd, buf, count);
    case MODE_FD_SELECT:
#if BUGLE_OSAPI_SELECT_BROKEN
        return gldb_protocol_reader_select_read(reader->select_reader, buf, count);
#else
        return read(reader->fd, buf, count);
#endif
    case MODE_FUNC:
        return reader->reader_func(reader->reader_arg, buf, count);
    }
    return -1; /* Should never be reached */
}

bugle_bool gldb_protocol_reader_has_data(gldb_protocol_reader *reader)
{
    if (reader->mode == MODE_FD_SELECT)
    {
#if BUGLE_OSAPI_SELECT_BROKEN
        return gldb_protocol_reader_select_has_data(reader->select_reader);
#else
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(reader->fd, &read_fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        select(reader->fd + 1, &read_fds, NULL, NULL, &timeout);
        return (FD_ISSET(reader->fd, &read_fds)) ? BUGLE_TRUE : BUGLE_FALSE;
#endif
    }
    else
        return BUGLE_FALSE;
}

/* FIXME: should eventually allow a user destructor for the user arg */
void gldb_protocol_reader_free(gldb_protocol_reader *reader)
{
#if BUGLE_OSAPI_SELECT_BROKEN
    if (reader->mode == MODE_FD_SELECT)
        gldb_protocol_reader_select_free(reader->select_reader);
#endif
    free(reader);
}

/* Returns BUGLE_FALSE on EOF, aborts on failure */
static bugle_bool io_safe_write(int fd, const void *buf, size_t count)
{
    ssize_t out;

    out = full_write(fd, buf, count);
    if (out < (ssize_t) count)
    {
        if (errno)
        {
            perror("write failed");
            exit(1);
        }
        return BUGLE_FALSE;
    }
    else
        return BUGLE_TRUE;
}

/* Returns BUGLE_FALSE on EOF, aborts on failure */
static bugle_bool io_safe_read(gldb_protocol_reader *reader, void *buf, size_t count)
{
    ssize_t bytes;

    while (count > 0)
    {
        bytes = gldb_protocol_reader_read(reader, buf, count);
        if (bytes <= 0)
        {
            if (errno)
            {
                perror("read failed");
                exit(1);
            }
            return BUGLE_FALSE;
        }
        count -= bytes;
        buf += bytes;
    }
    return BUGLE_TRUE;
}

bugle_bool gldb_protocol_send_code(int fd, uint32_t code)
{
    uint32_t code2;

    code2 = TO_NETWORK(code);
    return io_safe_write(fd, &code2, sizeof(uint32_t));
}

bugle_bool gldb_protocol_send_binary_string(int fd, uint32_t len, const char *str)
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
    if (!io_safe_write(fd, &len2, sizeof(uint32_t))) return BUGLE_FALSE;
    if (!io_safe_write(fd, str, len)) return BUGLE_FALSE;
    return BUGLE_TRUE;
}

bugle_bool gldb_protocol_send_string(int fd, const char *str)
{
    return gldb_protocol_send_binary_string(fd, strlen(str), str);
}

bugle_bool gldb_protocol_recv_code(gldb_protocol_reader *reader, uint32_t *code)
{
    uint32_t code2;
    if (io_safe_read(reader, &code2, sizeof(uint32_t)))
    {
        *code = TO_HOST(code2);
        return BUGLE_TRUE;
    }
    else
        return BUGLE_FALSE;
}

bugle_bool gldb_protocol_recv_binary_string(gldb_protocol_reader *reader, uint32_t *len, char **data)
{
    uint32_t len2;
    int old_errno;

    if (!io_safe_read(reader, &len2, sizeof(uint32_t))) return BUGLE_FALSE;
    *len = TO_HOST(len2);
    *data = bugle_malloc(*len + 1);
    if (!io_safe_read(reader, *data, *len))
    {
        old_errno = errno;
        free(*data);
        errno = old_errno;
        return BUGLE_FALSE;
    }
    else
    {
        (*data)[*len] = '\0';
        return BUGLE_TRUE;
    }
}

bugle_bool gldb_protocol_recv_string(gldb_protocol_reader *reader, char **str)
{
    uint32_t dummy;

    return gldb_protocol_recv_binary_string(reader, &dummy, str);
}
