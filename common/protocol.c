/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2009  Bruce Merry
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
#include "protocol.h"
#include <bugle/memory.h>
#include <bugle/porting.h>

/* Trying to detect whether or not there is a working htonl, what header
 * it is in, and what library to link against is far more effort than
 * simply writing it from scratch.
 */
static bugle_uint32_t io_htonl(bugle_uint32_t h)
{
    union
    {
        bugle_uint8_t bytes[4];
        bugle_uint32_t n;
    } u;

    u.bytes[0] = (h >> 24) & 0xff;
    u.bytes[1] = (h >> 16) & 0xff;
    u.bytes[2] = (h >> 8) & 0xff;
    u.bytes[3] = h & 0xff;
    return u.n;
}

static bugle_uint32_t io_ntohl(bugle_uint32_t n)
{
    union
    {
        bugle_uint8_t bytes[4];
        bugle_uint32_t n;
    } u;

    u.n = n;
    return (u.bytes[0] << 24) | (u.bytes[1] << 16) | (u.bytes[2] << 8) | u.bytes[3];
}

#define TO_NETWORK(x) io_htonl(x)
#define TO_HOST(x) io_ntohl(x)

bugle_bool gldb_protocol_send_code(bugle_io_writer *writer, bugle_uint32_t code)
{
    bugle_uint32_t code2;

    code2 = TO_NETWORK(code);
    if (bugle_io_write(&code2, sizeof(bugle_uint32_t), 1, writer) != 1) return BUGLE_FALSE;
    return BUGLE_TRUE;
}

bugle_bool gldb_protocol_send_binary_string(bugle_io_writer *writer, bugle_uint32_t len, const char *str)
{
    bugle_uint32_t len2;

    /* FIXME: on 64-bit systems, length could in theory be >=2^32. This is
     * not as unlikely as it sounds, since textures are retrieved in floating
     * point and so might be several times larger on the wire than on the
     * video card. If this happens, the logical thing is to make a packet size
     * of exactly 2^32-1 signal that a continuation packet will follow, which
     * avoid breaking backwards compatibility. Alternatively, it might be
     * better to just break backwards compatibility.
     */
    len2 = TO_NETWORK(len);
    if (bugle_io_write(&len2, sizeof(bugle_uint32_t), 1, writer) != 1) return BUGLE_FALSE;
    if (bugle_io_write(str, sizeof(char), len, writer) < len) return BUGLE_FALSE;
    return BUGLE_TRUE;
}

bugle_bool gldb_protocol_send_string(bugle_io_writer *writer, const char *str)
{
    return gldb_protocol_send_binary_string(writer, strlen(str), str);
}

bugle_bool gldb_protocol_recv_code(bugle_io_reader *reader, bugle_uint32_t *code)
{
    bugle_uint32_t code2;
    if (bugle_io_read(&code2, sizeof(code2), 1, reader) == 1)
    {
        *code = TO_HOST(code2);
        return BUGLE_TRUE;
    }
    else
        return BUGLE_FALSE;
}

bugle_bool gldb_protocol_recv_binary_string(bugle_io_reader *reader, bugle_uint32_t *len, char **data)
{
    bugle_uint32_t len2;

    if (bugle_io_read(&len2, sizeof(len2), 1, reader) != 1)
        return BUGLE_FALSE;
    *len = TO_HOST(len2);
    *data = bugle_malloc(*len + 1);
    if (bugle_io_read(*data, sizeof(char), *len, reader) != *len)
    {
        free(*data);
        return BUGLE_FALSE;
    }
    else
    {
        (*data)[*len] = '\0';
        return BUGLE_TRUE;
    }
}

bugle_bool gldb_protocol_recv_string(bugle_io_reader *reader, char **str)
{
    bugle_uint32_t dummy;

    return gldb_protocol_recv_binary_string(reader, &dummy, str);
}
