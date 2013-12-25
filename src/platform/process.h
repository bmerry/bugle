/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2013  Bruce Merry
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

#ifndef BUGLE_PLATFORM_PROCESS_H
#define BUGLE_PLATFORM_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bugle/export.h>
#include <bugle/bool.h>

/* Detects whether the currently executing process is a shell. This is
 * done heuristically. It is used to decide whether to run constructors.
 * If no determination could be made, false is returned.
 */
BUGLE_EXPORT_PRE bugle_bool bugle_process_is_shell(void) BUGLE_EXPORT_POST;

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_PLATFORM_PROCESS_H */
