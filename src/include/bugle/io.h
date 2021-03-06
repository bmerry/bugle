/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009-2010  Bruce Merry
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

#ifndef BUGLE_IO_H
#define BUGLE_IO_H

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <bugle/attributes.h>
#include <bugle/export.h>
#include <bugle/bool.h>
#include <bugle/porting.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bugle_io_reader bugle_io_reader;

/*** Reader input functions ***/

/* Read bytes from the stream.
 * Like fread, returns number of items read (0 if none available or on error).
 * However, it's possible that partial items will be read as well.
 *
 * This will keep trying to get all available data, so unlike read(2) it won't
 * return early just because the data isn't available yet.
 */
BUGLE_EXPORT_PRE size_t bugle_io_read(void *ptr, size_t size, size_t nmemb, bugle_io_reader *reader) BUGLE_EXPORT_POST;

/* Closes the underlying stream and cleans up.
 * Returns EOF on failure, 0 on success
 */
BUGLE_EXPORT_PRE int bugle_io_reader_close(bugle_io_reader *reader) BUGLE_EXPORT_POST;

typedef struct bugle_io_writer bugle_io_writer;

/*** Writer output functions ***/
/* These have the same semantics as fprintf, fputc, fwrite etc */
BUGLE_EXPORT_PRE int bugle_io_printf(bugle_io_writer *writer, const char *format, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(2, 3) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int bugle_io_vprintf(bugle_io_writer *writer, const char *format, va_list ap) BUGLE_ATTRIBUTE_FORMAT_PRINTF(2, 0) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int bugle_io_putc(int c, bugle_io_writer *writer) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int bugle_io_puts(const char *s, bugle_io_writer *writer) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE size_t bugle_io_write(const void *ptr, size_t size, size_t nmemb, bugle_io_writer *writer) BUGLE_EXPORT_POST;

/* Closes the underlying stream and clears up any memory.
 * Returns EOF on failure, 0 on success
 */
BUGLE_EXPORT_PRE int bugle_io_writer_close(bugle_io_writer *writer) BUGLE_EXPORT_POST;

/*** Specific types of writers */

/* Creates a writer that fills data into memory
 * Kills the program on OOM, so always returns non-NULL.
 */
BUGLE_EXPORT_PRE bugle_io_writer *bugle_io_writer_mem_new(size_t init_size) BUGLE_EXPORT_POST;

/* Obtains the string produced by a memory writer */
BUGLE_EXPORT_PRE char *bugle_io_writer_mem_get(const bugle_io_writer *writer) BUGLE_EXPORT_POST;

/* Gets the length of the string produced by a memory writer */
BUGLE_EXPORT_PRE size_t bugle_io_writer_mem_size(const bugle_io_writer *writer) BUGLE_EXPORT_POST;

/* Sets length to zero, but does not free memory */
BUGLE_EXPORT_PRE void bugle_io_writer_mem_clear(bugle_io_writer *writer) BUGLE_EXPORT_POST;

/* Frees the string memory. Do not use after this except to close */
BUGLE_EXPORT_PRE void bugle_io_writer_mem_release(bugle_io_writer *writer) BUGLE_EXPORT_POST;

/* Create a writer that wraps a FILE.
 * Kills the program on OOM, so always returns non-NULL.
 */
BUGLE_EXPORT_PRE bugle_io_writer *bugle_io_writer_file_new(FILE *f) BUGLE_EXPORT_POST;

/* Creates a writer to accept a connection on host:port. The interpretation of
 * host and port are platform-specific, but must accept at least an IPv4
 * address in host and a number in port. Host may also be NULL to bind to the
 * wildcard address.
 *
 * On success, the reader and writer are populated with the reader and writer
 * to use for input/output on the socket, and NULL is returned. On failure,
 * an error message is returned. This error message is dynamically allocated
 * and should be passed to bugle_free.
 */
BUGLE_EXPORT_PRE char *bugle_io_socket_listen(
    const char *host, const char *port,
    bugle_io_reader **reader, bugle_io_writer **writer) BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* BUGLE_IO_H */
