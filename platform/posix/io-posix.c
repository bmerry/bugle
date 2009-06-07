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
#include <sys/types.h>
#include <bugle/string.h>
#include <bugle/io.h>
#include <bugle/memory.h>
#include "platform/io.h"
#include "common/io-impl.h"

#define BUFFER_SIZE 256

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
    size_t total;

    /* TODO: handle multiplication overflow */
    total = size * nmemb;

    s = (bugle_io_writer_fd *) arg;
    while (written < total && -1 == (cur = write(s->fd, ptr, size * nmemb)))
    {
        if (errno != EINTR)
            return written / size;
        else
            written += cur;
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

#if HAVE_VA_COPY
    va_copy(ap2, ap);
#else
    memcpy(ap2, ap, sizeof(ap));
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
        free(buffer);
    }
    return ret;
}

static int fd_putc(int c, void *arg)
{
    char ch;
    ch = c;
    return fd_write(&ch, 1, 1, arg);
}

static int fd_close(void *arg)
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
    writer->fn_close = fd_close;
    writer->arg = s;

    s->fd = fd;

    return writer;
}
