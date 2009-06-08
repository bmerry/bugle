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

#ifndef BUGLE_PLATFORM_IO_H
#define BUGLE_PLATFORM_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bugle/io.h>
#include <bugle/export.h>

/* Wraps a file descriptor in a reader. The fd should be for a file or pipe,
 * not a network socket. Dies on malloc failure, so always returns non-NULL.
 */
BUGLE_EXPORT_PRE bugle_io_reader *bugle_io_reader_fd_new(int fd) BUGLE_EXPORT_POST;

/* Wraps a network socket in a reader. Dies on malloc failure, so always
 * returns non-NULL.
 */
BUGLE_EXPORT_PRE bugle_io_reader *bugle_io_reader_socket_new(int sock) BUGLE_EXPORT_POST;

/* Wraps a file descriptor in a writer. The fd should be for a file or pipe,
 * not a network socket. Dies on malloc failure, so always returns non-NULL.
 */
BUGLE_EXPORT_PRE bugle_io_writer *bugle_io_writer_fd_new(int fd) BUGLE_EXPORT_POST;

/* Wraps a network socket in a writer. Dies on malloc failure, so always
 * returns non-NULL.
 */
BUGLE_EXPORT_PRE bugle_io_writer *bugle_io_writer_socket_new(int sock) BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_PLATFORM_IO_H */
