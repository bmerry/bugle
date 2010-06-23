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

#ifndef BUGLE_COMMON_IO_IMPL_H
#define BUGLE_COMMON_IO_IMPL_H

#include <bugle/attributes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Defines structures that are used only in the implementation of the I/O
 * backends, but which are not part of the public interface.
 */

struct bugle_io_reader
{
    size_t (*fn_read)(void *ptr, size_t size, size_t nmemb, void *arg);
    int (*fn_close)(void *arg);

    void *arg;
};

struct bugle_io_writer
{
    /* vfprintf equivalent. If NULL, a fallback that formats via a string
     * followed by fn_write will be used.
     */
    int (*fn_vprintf)(void *arg, const char *format, va_list ap) BUGLE_ATTRIBUTE_FORMAT_PRINTF(2, 0);
    /* fputc equivalent. If NULL, fn_write will be used.
     */
    int (*fn_putc)(int c, void *arg);
    /* fwrite equivalent. Must not be NULL.
     */
    size_t (*fn_write)(const void *ptr, size_t size, size_t nmemb, void *arg);
    /* Function to close down the writer and free resources. Must not be NULL.
     */
    int (*fn_close)(void *arg);

    void *arg;  /* Value passed as the arg to the callbacks */
};

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_COMMON_IO_IMPL_H */
