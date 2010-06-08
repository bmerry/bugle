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
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <bugle/io.h>
#include <bugle/string.h>
#include <bugle/memory.h>
#include <bugle/attributes.h>
#include "io-impl.h"
#include "platform/macros.h"

size_t bugle_io_read(void *ptr, size_t size, size_t nmemb, bugle_io_reader *reader)
{
    return reader->fn_read(ptr, size, nmemb, reader->arg);
}

bugle_bool bugle_io_reader_has_data(bugle_io_reader *reader)
{
    if (reader->fn_has_data != NULL)
        return reader->fn_has_data(reader->arg);
    else
        return BUGLE_FALSE;
}

int bugle_io_reader_close(bugle_io_reader *reader)
{
    int ret = reader->fn_close(reader->arg);
    free(reader);
    return ret;
}

typedef struct bugle_io_writer_mem
{
    char *ptr;
    size_t size;        /* written (excluding terminating null) */
    size_t total;       /* size from malloc */
} bugle_io_writer_mem;

int bugle_io_printf(bugle_io_writer *writer, const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    if (writer->fn_vprintf != NULL)
        ret = writer->fn_vprintf(writer->arg, format, ap);
    else
        ret = bugle_io_vprintf(writer, format, ap);
    va_end(ap);
    return ret;
}

int bugle_io_vprintf(bugle_io_writer *writer, const char *format, va_list ap)
{
    if (writer->fn_vprintf != NULL)
        return writer->fn_vprintf(writer->arg, format, ap);
    else
    {
        int length;
        char small_buffer[128];
        char *buffer;
        int ret;
        va_list ap2;

        /* Back up the arg list, since if we fall back to a second pass we'll
         * need it.
         */
        BUGLE_VA_COPY(ap2, ap);
        length = bugle_vsnprintf(small_buffer, sizeof(small_buffer), format, ap);
        if (length < sizeof(small_buffer))
        {
            ret = writer->fn_write(small_buffer, sizeof(char), length, writer->arg);
        }
        else
        {
            buffer = bugle_vasprintf(format, ap2);
            ret = writer->fn_write(buffer, sizeof(char), length, writer->arg);
            bugle_free(buffer);
        }
        va_end(ap2);
        return ret;
    }
}

size_t bugle_io_write(const void *ptr, size_t size, size_t nmemb, bugle_io_writer *writer)
{
    return writer->fn_write(ptr, size, nmemb, writer->arg);
}

int bugle_io_putc(int c, bugle_io_writer *writer)
{
    if (writer->fn_putc != NULL)
        return writer->fn_putc(c, writer->arg);
    else
    {
        char ch = c;
        return writer->fn_write(&ch, 1, 1, writer->arg);
    }
}

int bugle_io_puts(const char *s, bugle_io_writer *writer)
{
    return writer->fn_write(s, sizeof(char), strlen(s), writer->arg);
}

int bugle_io_writer_close(bugle_io_writer *writer)
{
    int ret;
    ret = writer->fn_close(writer->arg);
    bugle_free(writer);
    return ret;
}

static int mem_vprintf(void *arg, const char *format, va_list ap)
{
    bugle_io_writer_mem *mem;
    int written;
    va_list ap2;    /* backup copy, for second pass */

    BUGLE_VA_COPY(ap2, ap);

    mem = (bugle_io_writer_mem *) arg;
    written = bugle_vsnprintf(mem->ptr + mem->size, mem->total - mem->size, format, ap);
    assert(written >= 0);   /* bugle_vsnprintf is supposed to be C99-like */
    if (written >= mem->total - mem->size)
    {
        mem->total *= 2;    /* at least double, to avoid lots of allocs */
        if (written >= mem->total - mem->size)
            mem->total = written + mem->size + 1; /* enough room for this one */
        mem->ptr = BUGLE_NREALLOC(mem->ptr, mem->total, char);

        written = bugle_vsnprintf(mem->ptr + mem->size, mem->total - mem->size, format, ap2);
        assert(written >= 0 && written < mem->total - mem->size);
    }
    va_end(ap2);
    mem->size += written;
    return written;
}

static int mem_putc(int c, void *arg)
{
    bugle_io_writer_mem *mem;

    mem = (bugle_io_writer_mem *) arg;
    if (mem->size + 1 >= mem->total)
    {
        mem->total *= 2;
        mem->ptr = BUGLE_NREALLOC(mem->ptr, mem->total, char);
    }
    mem->ptr[mem->size++] = (char) c;
    mem->ptr[mem->size] = '\0';
    return c;
}

static size_t mem_write(const void *ptr, size_t size, size_t nmemb, void *arg)
{
    bugle_io_writer_mem *mem;

    mem = (bugle_io_writer_mem *) arg;
    size *= nmemb;  /* FIXME: check for overflow */
    if (size >= mem->total - mem->size)
    {
        mem->total *= 2;
        if (size >= mem->total - mem->size)
            mem->total = size + mem->size + 1;
        mem->ptr = BUGLE_NREALLOC(mem->ptr, mem->total, char);
    }
    memcpy(mem->ptr + mem->size, ptr, size);
    mem->size += size;
    mem->ptr[mem->size] = '\0';
    return nmemb;
}

static int mem_close(void *arg)
{
    /* Do nothing. The user is responsible for freeing the string */
    return 0;
}

bugle_io_writer *bugle_io_writer_mem_new(size_t init_size)
{
    bugle_io_writer *writer;
    bugle_io_writer_mem *mem;

    writer = BUGLE_MALLOC(bugle_io_writer);
    mem = BUGLE_MALLOC(bugle_io_writer_mem);

    writer->fn_vprintf = mem_vprintf;
    writer->fn_putc = mem_putc;
    writer->fn_write = mem_write;
    writer->fn_close = mem_close;
    writer->arg = mem;

    mem->size = 0;
    mem->total = init_size;
    mem->ptr = BUGLE_NMALLOC(mem->total, char);
    mem->ptr[0] = '\0';

    return writer;
}

char *bugle_io_writer_mem_get(const bugle_io_writer *writer)
{
    return ((const bugle_io_writer_mem *) writer->arg)->ptr;
}

size_t bugle_io_writer_mem_size(const bugle_io_writer *writer)
{
    return ((const bugle_io_writer_mem *) writer->arg)->size;
}

void bugle_io_writer_mem_clear(bugle_io_writer *writer)
{
    bugle_io_writer_mem *mem;

    mem = (bugle_io_writer_mem *) writer->arg;
    mem->size = 0;
    mem->ptr[0] = '\0';
}

void bugle_io_writer_mem_release(bugle_io_writer *writer)
{
    bugle_io_writer_mem *mem;

    mem = (bugle_io_writer_mem *) writer->arg;
    bugle_free(mem->ptr);
    mem->ptr = NULL;
    mem->size = 0;
    mem->total = 0;
}

bugle_io_writer *bugle_io_writer_file_new(FILE *f)
{
    bugle_io_writer *writer;

    writer = BUGLE_MALLOC(bugle_io_writer);

    writer->fn_vprintf = (int (*)(void *, const char *, va_list)) vfprintf;
    writer->fn_putc = (int (*)(int, void *)) fputc;
    writer->fn_write = (size_t (*)(const void *, size_t, size_t, void *)) fwrite;
    writer->fn_close = (int (*)(void *)) fclose;
    writer->arg = f;
    return writer;
}
