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

#ifndef BUGLE_BUDGIE_REFLECT_H
#define BUGLE_BUDGIE_REFLECT_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <budgie/types.h>

int             budgie_function_count(void);
const char *    budgie_function_name(budgie_function id);
budgie_function budgie_function_id(const char *name);
void          (*budgie_function_address(budgie_function id, bool real))(void);
budgie_group    budgie_function_group(budgie_function id);

budgie_group    budgie_group_id(const char *name); /* Takes a function name from the group */
int             budgie_group_parameter_count(budgie_group id);
/* Query -1 for the return type; void is returned as NULL_TYPE */
budgie_type     budgie_group_parameter_type(budgie_group id, int param);

/* Returns the type that is a pointer to "type", or NULL_TYPE if the
 * type was not found when running budgie.
 */
budgie_type     budgie_type_pointer(budgie_type type);
/* Returns the type that this type points to, if NULL_TYPE is this type
 * is not a pointer type.
 */
budgie_type     budgie_type_pointer_base(budgie_type type);
size_t          budgie_type_size(budgie_type type);
const char *    budgie_type_name(budgie_type type);
budgie_type     budgie_type_id(const char *name);

void budgie_dump_any_type(budgie_type type, const void *value, int length, FILE *out);
/* Calls budgie_dump_any_type, BUT:
 * if outer_length != -1, then it considers the type as if it were an
 * array of that type, of length outer_length. If pointer is
 * non-NULL, then it outputs "<pointer> -> " first as if dumping a
 * pointer to the array.
 */
void budgie_dump_any_type_extended(budgie_type type,
                                   const void *value,
                                   int length,
                                   int outer_length,
                                   const void *pointer,
                                   FILE *out);

/* Generated function that converts [an array of] one numeric type to another */
void budgie_type_convert(void *out, budgie_type out_type, const void *in, budgie_type in_type, size_t count);

#endif /* BUGLE_BUDGIE_REFLECT_H */
