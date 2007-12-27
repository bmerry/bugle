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
const char *    budgie_type_name(budgie_type type); /* e.g. "PK6GLenum" */
const char *    budgie_type_name_nomangle(budgie_type type); /* e.g. "const GLenum *" */
budgie_type     budgie_type_id(const char *name);
budgie_type     budgie_type_id_nomangle(const char *name);

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

/* Some black magic to look up ID's as efficiently as possible. If defines.h
 * is included, GCC will compile this down to a constant. If it is not
 * included, it boils down to a cached lookup. It might or might not be
 * more efficient to have a symbol in libbugleutils.so for every
 * function/group/type that is defined to avoid doing the hash lookup for
 * filter-sets, but keep in mind that the dynamic linker will do a similar
 * lookup anyway. The pros and cons of this method are:
 *
 * Pros:
 * - After the first lookup, only a static variable needs referencing, rather
 * than an indirect reference through the GOT.
 * - This method is not prone to changes in bugle itself: if bugle ceases to
 * define the symbol there would be problems. A weak symbol would probably
 * solve this, but I don't know if it can be done without then introducing an
 * address check on every access (at least within the macro).
 *
 * Cons
 * - A cache variable is needed per lexical use, rather than per symbol.
 * - The cache miss in incurred the first time for each static use, rather
 * than the first time for the symbol.
 */
/* This particular test is taken from glib/gmacros.h */
#if 0 && defined (__GNUC__) && !defined(__STRICT_ANSI__) && !defined(__cplusplus)
#define _BUDGIE_PASTE_L(x) x ## L
#define _BUDGIE_ID_FULL(type, call, fullname, namestr) \
    __extension__ ({ static type fullname ## L = -2; \
    ((type) _BUDGIE_PASTE_L(fullname) == -2) ? \
        (fullname ## L = call((namestr))) : \
        (type) _BUDGIE_PASTE_L(fullname); })
#define BUDGIE_FUNCTION_ID(name) \
    _BUDGIE_ID_FULL(budgie_function, budgie_function_id, FUNC_ ## name, #name)
#define BUDGIE_TYPE_ID(name) \
    _BUDGIE_ID_FULL(budgie_type, budgie_type_id, TYPE_ ## name, #name)
#define BUDGIE_GROUP_ID(name) \
    _BUDGIE_ID_FULL(budgie_group, budgie_group_id, GROUP_ ## name, #name)
#else
#define BUDGIE_FUNCTION_ID(name) (budgie_function_id(#name))
#define BUDGIE_GROUP_ID(name) (budgie_group_id(#name))
#define BUDGIE_TYPE_ID(name) (budgie_type_id(#name))
#endif

#endif /* BUGLE_BUDGIE_REFLECT_H */
