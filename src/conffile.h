/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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

#ifndef BUGLE_SRC_CONFFILE_H
#define BUGLE_SRC_CONFFILE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "gl_list.h"

typedef struct
{
    char *name;
    char *value;
} config_variable;

typedef struct
{
    char *name;
    gl_list_t variables;
    char *key;
    int active;
} config_filterset;

typedef struct
{
    char *name;
    gl_list_t filtersets;
} config_chain;

void bugle_config_destroy(void);
const config_chain *bugle_config_get_chain(const char *name);
gl_list_t bugle_config_get_root(void);

#endif /* !BUGLE_SRC_CONFFILE_H */
