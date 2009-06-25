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

#include "platform/dl.h"

void bugle_dl_init(void)
{
}

bugle_dl_module bugle_dl_open(const char *filename, int flag)
{
    return NULL;
}

int bugle_dl_close(bugle_dl_module module)
{
    return 0;
}

void *bugle_dl_sym_data(bugle_dl_module module, const char *symbol)
{
    return NULL;
}

BUDGIEAPIPROC bugle_dl_sym_function(bugle_dl_module module, const char *symbol)
{
    return (BUDGIEAPIPROC) NULL;
}

void bugle_dl_foreach(const char *path, void (*callback)(const char *filename, void *arg), void *arg)
{
}

