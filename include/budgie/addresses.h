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

#ifndef BUGLE_BUDGIE_ADDRESSES_H
#define BUGLE_BUDGIE_ADDRESSES_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdbool.h>
#include <budgie/types.h>

/* Dumps a call, including arguments and return. Does not include a newline */
void budgie_dump_any_call(const generic_function_call *call, int indent, FILE *out);

void (*budgie_function_address_real(budgie_function id))(void);
void (*budgie_function_address_wrapper(budgie_function id))(void);

/* These should only be used by the interceptor - it sets up all the addresses
 * (real and wrapper) for budgie_function_address.
 */
void budgie_function_address_initialise(void);
/* Used to manually override the initialiser */
void budgie_function_address_set_real(budgie_function id, void (*addr)(void));

void budgie_function_set_bypass(budgie_function id, bool bypass);

void budgie_invoke(function_call *call);

#include <budgie/call.h>

/* Expands to ifdef if def is defined, to ifndef otherwise */
#define _BUDGIE_SWITCH(def, ifdef, ifndef) _BUDGIE_SWITCH2(def, ifdef, ifndef)
#define _BUDGIE_SWITCH2(def, ifdef, ifndef) \
    _BUDGIE_SWITCH3(_BUDGIE_SWITCH_PRE ## def(ifdef), (ifndef), _BUDGIE_SWITCH_POST ## def(ifdef))
#define _BUDGIE_SWITCH3(pre, ifndef, post) _BUDGIE_SWITCH_2ND(pre, ifndef, post)
#define _BUDGIE_SWITCH_2ND(a, b, c) b
#define _BUDGIE_SWITCH_PRE1(ifdef) 0, (ifdef), (
#define _BUDGIE_SWITCH_POST1(ifdef) )

/* the generated calls.h generates
 * _BUDGIE_CALL_glFoo => BUDGIE_CALL(glFoo, typeof(glFoo)) */
#define CALL(fn) _BUDGIE_SWITCH(_BUDGIE_HAVE_CALL_ ## fn, _BUDGIE_CALL_ ## fn, fn)

/* Handles the case where the symbol was defined in the header at compilation
 * time, but libbugleutils has been downgraded and it is no longer available
 * at run-time.
 */
static inline void (*_budgie_function_address_get_real(budgie_function id, void (*symbol)(void)))(void)
{
    return id == -1 ? symbol : budgie_function_address_real(id);
}
#define _BUDGIE_CALL(name, type) \
    ((type) _budgie_function_address_get_real(BUDGIE_FUNCTION_ID(name), (void (*)(void)) name))

#endif /* BUGLE_BUDGIE_ADDRESSES_H */
