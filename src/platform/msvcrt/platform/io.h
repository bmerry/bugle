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

#ifndef BUGLE_PLATFORM_IO_H
#define BUGLE_PLATFORM_IO_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <io.h>
#include <fcntl.h>
#include "platform_config.h"
#include <bugle/io.h>
#include <bugle/export.h>
#include <bugle/bool.h>

/* Create a reader that wraps a file HANDLE. Do not use this for sockets.
 * Kills the program on OOM, so always returns non-NULL.
 *
 * If do_close is BUGLE_TRUE, the handle will be closed when the reader is
 * closed.
 */
BUGLE_EXPORT_PRE bugle_io_reader *bugle_io_reader_handle_new(HANDLE handle, bugle_bool do_close) BUGLE_EXPORT_POST;

/* Create a writer that wraps a file HANDLE. Do not use this for sockets.
 * Kills the program on OOM, so always returns non-NULL.
 *
 * If do_close is BUGLE_TRUE, the handle will be closed when the writer is
 * closed.
 */
BUGLE_EXPORT_PRE bugle_io_writer *bugle_io_writer_handle_new(HANDLE handle, bugle_bool do_close) BUGLE_EXPORT_POST;

/* Create a reader that wraps a Winsock socket. Kills the program on OOM, so
 * always returns non-NULL.
 *
 * If do_close is BUGLE_TRUE, the handle will be closed when the reader is
 * closed.
 */
BUGLE_EXPORT_PRE bugle_io_reader *bugle_io_reader_socket_new(SOCKET sock, bugle_bool do_close) BUGLE_EXPORT_POST;

/* Create a writer that wraps a Winsock socket. Kills the program on OOM, so
 * always returns non-NULL.
 *
 * If do_close is BUGLE_TRUE, the handle will be closed when the writer is
 * closed.
 */
BUGLE_EXPORT_PRE bugle_io_writer *bugle_io_writer_socket_new(SOCKET sock, bugle_bool do_close) BUGLE_EXPORT_POST;

#endif /* !BUGLE_PLATFORM_IO_H */
