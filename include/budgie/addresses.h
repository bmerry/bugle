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

#ifndef BUGLE_BUDGIE_ADDRESSES_H
#define BUGLE_BUDGIE_ADDRESSES_H

#include <stdio.h>
#include <bugle/bool.h>
#include <budgie/types.h>
#include <bugle/export.h>
#include <bugle/io.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Determines the length of the array pointed to by a parameter. If it is
 * not an array (e.g. non-pointer, or pointer to a single item), returns -1.
 * Use -1 as the parameter index to query the return.
 */
BUGLE_EXPORT_PRE budgie_type budgie_call_parameter_type(const generic_function_call *call, int param) BUGLE_EXPORT_POST;
/* Dumps one parameter from a call, with the appropriate type and length
 * as determined by the parameters. Use -1 for the return (which must then
 * exist).
 */
BUGLE_EXPORT_PRE int budgie_call_parameter_length(const generic_function_call *call, int param) BUGLE_EXPORT_POST;
/* Dumps a single parameter, with appropriate type and length */
BUGLE_EXPORT_PRE void budgie_call_parameter_dump(const generic_function_call *call, int param, bugle_io_writer *writer) BUGLE_EXPORT_POST;
/* Dumps a call, including arguments and return. Does not include a newline */
BUGLE_EXPORT_PRE void budgie_dump_any_call(const generic_function_call *call, int indent, bugle_io_writer *writer) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE BUDGIEAPIPROC budgie_function_address_real(budgie_function id) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE BUDGIEAPIPROC budgie_function_address_wrapper(budgie_function id) BUGLE_EXPORT_POST;

/* These should only be used by the interceptor - it sets up all the addresses
 * (real and wrapper) for budgie_function_address_*.
 */
BUGLE_EXPORT_PRE void budgie_function_address_initialise(void) BUGLE_EXPORT_POST;
/* Used to manually override the initialiser */
BUGLE_EXPORT_PRE void budgie_function_address_set_real(budgie_function id, void (BUDGIEAPI *addr)(void)) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE void budgie_function_set_bypass(budgie_function id, bugle_bool bypass) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE void budgie_invoke(function_call *call) BUGLE_EXPORT_POST;

#include <budgie/call.h>
#include <budgie/macros.h>

/* the generated calls.h generates
 * _BUDGIE_CALL_glFoo => BUDGIE_CALL(glFoo, typeof(glFoo)) */
#define CALL(fn) _BUDGIE_SWITCH(_BUDGIE_HAVE_CALL_ ## fn, _BUDGIE_CALL_ ## fn, fn)

/* Handles the case where the symbol was defined in the header at compilation
 * time, but libbugleutils has been downgraded and it is no longer available
 * at run-time.
 */
static inline void (BUDGIEAPI *_budgie_function_address_get_real(budgie_function id, void (BUDGIEAPI *symbol)(void)))(void)
{
    return id == -1 ? symbol : budgie_function_address_real(id);
}

/* Note: it's tempting to put to put 'name' instead of NULL (and this was
 * originally done). However, there is no guarantee that the symbol will
 * actually exist (unless bugle overrides it, in which case BUDGIE_FUNCTION_ID
 * will succeed anyway), and undefined symbols are not always valid.
 */
#define _BUDGIE_CALL(name, type) \
    ((type) _budgie_function_address_get_real(BUDGIE_FUNCTION_ID(name), (void (BUDGIEAPI *)(void)) NULL))

#ifdef __cplusplus
}
#endif

#endif /* BUGLE_BUDGIE_ADDRESSES_H */
