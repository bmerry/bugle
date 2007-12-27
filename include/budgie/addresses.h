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

#endif /* BUGLE_BUDGIE_ADDRESSES_H */
