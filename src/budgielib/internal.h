/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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

#ifndef BUGLE_BUDGIE_INTERNAL_H
#define BUGLE_BUDGIE_INTERNAL_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <budgie/types.h>
#include <bugle/export.h>
#include <bugle/io.h>

typedef struct
{
    budgie_type type;
} group_parameter_data;

typedef struct
{
    size_t num_parameters;
    const budgie_type *parameter_types;
    budgie_type retn_type;
    bugle_bool has_retn;
} group_data;

typedef struct
{
    const char *name;
    budgie_group group;
    budgie_function next_in_group;  /* circular linked list */
} function_data;

typedef struct
{
    const char *name;
    budgie_function function;
} function_name_data;

typedef enum
{
    CODE_ENUMERAL,
    CODE_INTEGRAL,
    CODE_FLOAT,
    CODE_COMPLEX,
    CODE_POINTER,
    CODE_ARRAY,
    CODE_RECORD,
    CODE_OTHER
} type_code;

typedef struct
{
    budgie_type type;
    ptrdiff_t offset;
} type_record_data;

typedef void (*type_dumper)(const void *, int, bugle_io_writer *writer);
typedef budgie_type (*type_get_type)(const void *);
typedef int (*type_get_length)(const void *);

typedef struct
{
    const char *name_nomangle;
    const char *name;
    type_code code;
    budgie_type type;    /* base type of pointers, arrays, functions etc */
    budgie_type pointer; /* pointer to this type (NULL_TYPE if not generated) */
    const type_record_data *fields;
    size_t size;
    size_t length;
    type_dumper dumper;
    type_get_type get_type;
    type_get_length get_length;
} type_data;

typedef struct
{
    unsigned int value;
    const char *name;
} bitfield_pair;

extern const type_data _budgie_type_table[];
extern int _budgie_type_count;

extern int _budgie_function_count;
extern const function_data _budgie_function_table[];
extern const function_name_data _budgie_function_name_table[]; /* Holds wrappers in alphabetical order */
extern bugle_bool _budgie_bypass[];

extern int _budgie_group_count;
extern const group_data _budgie_group_table[];

void _budgie_dump_bitfield(unsigned int value, bugle_io_writer *writer,
                           const bitfield_pair *tags, int count);

/* User functions for .bc files */

bugle_bool BUGLE_EXPORT_PRE budgie_dump_string(const char *value, bugle_io_writer *writer) BUGLE_EXPORT_POST;
int BUGLE_EXPORT_PRE budgie_count_string(const char *value) BUGLE_EXPORT_POST;
/* Like dump_string but takes an explicit length rather than NULL terminator */
bugle_bool BUGLE_EXPORT_PRE budgie_dump_string_length(const char *value, size_t length, bugle_io_writer *writer) BUGLE_EXPORT_POST;

#endif /* BUGLE_BUDGIE_INTERNAL_H */
