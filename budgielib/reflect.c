/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007, 2009  Bruce Merry
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ltdl.h>
#include <bugle/hashtable.h>
#include <bugle/string.h>
#include <bugle/io.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include "internal.h"
#include "platform/threads.h"

static hash_table function_id_map;
static hash_table type_id_map;
static hash_table type_id_nomangle_map;
bugle_thread_once_define(static, reflect_once)

static void reflect_shutdown(void)
{
    bugle_hash_clear(&function_id_map);
    bugle_hash_clear(&type_id_map);
    bugle_hash_clear(&type_id_nomangle_map);
}

static void reflect_initialise(void)
{
    int i;

    bugle_hash_init(&function_id_map, NULL);
    for (i = 0; i < _budgie_function_count; i++)
        bugle_hash_set(&function_id_map, _budgie_function_table[i].name,
                       (void *) (size_t) (i + 1));

    bugle_hash_init(&type_id_map, NULL);
    bugle_hash_init(&type_id_nomangle_map, NULL);
    for (i = 0; i < _budgie_type_count; i++)
    {
        bugle_hash_set(&type_id_map, _budgie_type_table[i].name,
                       (void *) (size_t) (i + 1));
        bugle_hash_set(&type_id_nomangle_map, _budgie_type_table[i].name_nomangle,
                       (void *) (size_t) (i + 1));
    }

    atexit(reflect_shutdown);
}

int budgie_function_count()
{
    return _budgie_function_count;
}

const char *budgie_function_name(budgie_function id)
{
    assert(id >= 0 && id < _budgie_function_count);
    return _budgie_function_table[id].name;
}

budgie_function budgie_function_id(const char *name)
{
    bugle_thread_once(reflect_once, reflect_initialise);
    return (budgie_function) (size_t) bugle_hash_get(&function_id_map, name) - 1;
}

budgie_group budgie_function_group(budgie_function id)
{
    assert(id >= 0 && id < _budgie_function_count);
    return _budgie_function_table[id].group;
}

budgie_function budgie_function_next(budgie_function id)
{
    assert(id >= 0 && id < _budgie_function_count);
    return _budgie_function_table[id].next_in_group;
}


int budgie_group_count(void)
{
    return _budgie_group_count;
}

int budgie_group_parameter_count(budgie_group id)
{
    assert(id >= 0 && id < _budgie_group_count);
    return _budgie_group_table[id].num_parameters;
}

budgie_type budgie_group_parameter_type(budgie_group id, int param)
{
    assert(id >= 0 && id < _budgie_group_count);
    if (param == -1)
        return _budgie_group_table[id].has_retn
            ? _budgie_group_table[id].retn_type : NULL_TYPE;
    else
    {
        assert(0 <= param && param < (int) _budgie_group_table[id].num_parameters);
        return _budgie_group_table[id].parameter_types[param];
    }
}

budgie_group budgie_group_id(const char *name)
{
    budgie_function f;
    f = budgie_function_id(name);
    return f == NULL_FUNCTION ? NULL_GROUP : budgie_function_group(f);
}


int budgie_type_count(void)
{
    return _budgie_type_count;
}

budgie_type budgie_type_pointer(budgie_type type)
{
    assert(type >= 0 && type < _budgie_type_count);
    return _budgie_type_table[type].pointer;
}

budgie_type budgie_type_pointer_base(budgie_type type)
{
    assert(type >= 0 && type < _budgie_type_count);
    return _budgie_type_table[type].code == CODE_POINTER
        ? _budgie_type_table[type].type : NULL_TYPE;
}

size_t budgie_type_size(budgie_type type)
{
    assert(type >= 0 && type < _budgie_type_count);
    return _budgie_type_table[type].size;
}

const char *budgie_type_name(budgie_type type)
{
    assert(type >= 0 && type < _budgie_type_count);
    return _budgie_type_table[type].name;
}

const char *budgie_type_name_nomangle(budgie_type type)
{
    assert(type >= 0 && type < _budgie_type_count);
    return _budgie_type_table[type].name_nomangle;
}

budgie_type budgie_type_id(const char *name)
{
    bugle_thread_once(reflect_once, reflect_initialise);
    return (budgie_type) (size_t) bugle_hash_get(&type_id_map, name) - 1;
}

budgie_type budgie_type_id_nomangle(const char *name)
{
    bugle_thread_once(reflect_once, reflect_initialise);
    return (budgie_type) (size_t) bugle_hash_get(&type_id_nomangle_map, name) - 1;
}

/* Remaps a type based on the instance. This is not expected to be used; it
 * will be more common to remap for specific function parameters.
 */
budgie_type budgie_type_type(budgie_type type, const void *instance)
{
    if (type < 0 || type >= _budgie_type_count)
        return type; /* Graceful degradation */
    if (_budgie_type_table[type].get_type)
    {
        budgie_type new_type;
        new_type = _budgie_type_table[type].get_type(instance);
        if (new_type != type)
            return budgie_type_type(new_type, instance);
    }
    return type;
}

int budgie_type_length(budgie_type type, const void *instance)
{
    type = budgie_type_type(type, instance);
    if (type >= 0 && type < _budgie_type_count
        && _budgie_type_table[type].get_length)
        return _budgie_type_table[type].get_length(instance);
    else
        return -1;
}

void budgie_dump_any_type(budgie_type type, const void *value, int length, bugle_io_writer *writer)
{
    const type_data *info;

    type = budgie_type_type(type, value);
    assert(type >= 0);

    info = &_budgie_type_table[type];
    /* Note that we don't override the length if specified, since it may come
     * from a parameter override which would have better knowledge.
     */
    if (info->get_length && length == -1) 
        length = (*info->get_length)(value);

    assert(info->dumper);
    (*info->dumper)(value, length, writer);
}

void budgie_dump_any_type_extended(budgie_type type,
                                   const void *value,
                                   int length,
                                   int outer_length,
                                   const void *pointer,
                                   bugle_io_writer *writer)
{
    int i;
    const char *v;

    if (pointer)
        bugle_io_printf(writer, "%p -> ", pointer);
    if (outer_length == -1)
        budgie_dump_any_type(type, value, length, writer);
    else
    {
        v = (const char *) value;
        bugle_io_puts("{ ", writer);
        for (i = 0; i < outer_length; i++)
        {
            if (i) bugle_io_puts(", ", writer);
            budgie_dump_any_type(type, (const void *) v, length, writer);
            v += _budgie_type_table[type].size;
        }
        bugle_io_puts(" }", writer);
    }
}
