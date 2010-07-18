/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007, 2010  Bruce Merry
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

#ifndef BUGLE_BUDGIE_MACROS_H
#define BUGLE_BUDGIE_MACROS_H

#if defined (__GNUC__) && !defined(__STRICT_ANSI__) && !defined(__cplusplus)
# define _BUDGIE_PASTE_L(x) x ## L
# define _BUDGIE_ID_FULL(type, call, fullname, namestr) \
    __extension__ ({ static type fullname ## L = -2; \
    ((type) _BUDGIE_PASTE_L(fullname) == -2) ? \
        (fullname ## L = call((namestr))) : \
        (type) _BUDGIE_PASTE_L(fullname); })
#else
# define _BUDGIE_ID_FULL(type, call, fullname, namestr) (call((namestr)))
#endif

/* Expands to ifdef if def is defined to 1, and to ifndef otherwise */

/* First force the arguments to be expanded, so we paste with the defined
 * value if any.
 */
#define _BUDGIE_SWITCH(def, ifdef, ifndef) _BUDGIE_SWITCH2(def, ifdef, ifndef)

/* If the define is not set, then
 * _BUDGIE_SWITCH_LEFT ## def, ifndef, _BUDGIE_SWITCH_RIGHT ## def(ifdef)
 * is just what it looks like, with 'ifndef' being the middle argument.
 * If def is defined to 1, then the macros insert an extra layer of
 * brackets around the visible commas, and add two extra arguments to the
 * right, changing what constitutes the middle argument.
 */
#define _BUDGIE_SWITCH2(def, ifdef, ifndef) \
    _BUDGIE_SWITCH3((_BUDGIE_SWITCH_LEFT ## def, ifndef, _BUDGIE_SWITCH_RIGHT ## def(ifdef)))
#define _BUDGIE_SWITCH_LEFT1 (dummy
#define _BUDGIE_SWITCH_RIGHT1(ifdef) ), ifdef, dummy 

/* The call to _BUDGIE_SWITCH3 has two layers of brackets, otherwise the
 * commas are detected too early. Strip off the outer layer now.
 */
#define _BUDGIE_SWITCH3(x) _BUDGIE_SWITCH4 x

/* And finally, select the middle element
 */
#define _BUDGIE_SWITCH4(a, b, c) (b)

#endif /* !BUGLE_BUDGIE_MACROS_H */
