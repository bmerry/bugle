/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include "canon.h"
#include "src/glfuncs.h"
#include "src/utils.h"
#include "common/bool.h"
#include "common/safemem.h"
#include "common/hashtable.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static hash_table function_names;

void destroy_canonical(void)
{
    hash_clear(&function_names, false);
}

void initialise_canonical(void)
{
    budgie_function i;

    hash_init(&function_names);
    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        hash_set(&function_names, function_table[i].name, &function_table[i]);

    atexit(destroy_canonical);
}

budgie_function canonical_function(budgie_function f)
{
    if (f < 0 || f >= NUMBER_OF_FUNCTIONS)
        return f;
    else
        return gl_function_table[f].canonical;
}

budgie_function canonical_call(const function_call *call)
{
    if (call->generic.id < 0 || call->generic.id >= NUMBER_OF_FUNCTIONS)
        return call->generic.id;
    else
        return gl_function_table[call->generic.id].canonical;
}

budgie_function find_function(const char *name)
{
    const function_data *alias;

    alias = hash_get(&function_names, name);
    if (alias) return alias - function_table;
    else return NULL_FUNCTION;
}
