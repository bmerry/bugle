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

#ifndef BUDGIE_LIB_H
#define BUDGIE_LIB_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <budgie/types2.h>

#if BUGLE_HAVE_ATTRIBUTE_HIDDEN_ALIAS
# define BUGLE_ATTRIBUTE_HIDDEN_ALIAS(f) bugle_hidden_alias_ ## f
# define BUGLE_ATTRIBUTE_DECLARE_HIDDEN_ALIAS(f) __typeof(f) BUGLE_ATTRIBUTE_HIDDEN_ALIAS(f);
# define BUGLE_ATTRIBUTE_DEFINE_HIDDEN_ALIAS(f) extern __typeof(f) BUGLE_ATTRIBUTE_HIDDEN_ALIAS(f) __attribute__((alias(#f), visibility("hidden")));
#else
# define BUGLE_ATTRIBUTE_HIDDEN_ALIAS(f) f
# define BUGLE_ATTRIBUTE_DECLARE_HIDDEN_ALIAS(f)
# define BUGLE_ATTRIBUTE_DEFINE_HIDDEN_ALIAS(f)
#endif

/* Holds function pointers for real implementations */
extern void (BUDGIEAPI *_budgie_function_address_real[])(void);
/* Holds function pointers for wrappers */
extern void (BUDGIEAPI *_budgie_function_address_wrapper[])(void);

extern int _budgie_library_count;
extern const char * const _budgie_library_names[];

extern bool _budgie_bypass[];

/* Code used by generated lib.c code */

bool _budgie_reentrance_init(void);
void _budgie_reentrance_clear(void);

void budgie_interceptor(function_call *call);


typedef bool (*arg_dumper)(const generic_function_call *, int, const void *, int, char **buffer, size_t *size);
typedef budgie_type (*arg_get_type)(const generic_function_call *, int, const void *);
typedef int (*arg_get_length)(const generic_function_call *, int, const void *);

typedef struct
{
    budgie_type type;
    arg_dumper dumper;
    arg_get_type get_type;
    arg_get_length get_length;
} group_dump_parameter;

typedef struct
{
    size_t num_parameters;
    const group_dump_parameter *parameters;
    group_dump_parameter retn;
    bool has_retn;
} group_dump;

extern const group_dump _budgie_group_dump_table[];

#endif /* BUDGIE_LIB_H */
