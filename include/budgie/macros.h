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

/* Expands to ifdef if def is defined, to ifndef otherwise */
#define _BUDGIE_SWITCH(def, ifdef, ifndef) _BUDGIE_SWITCH2(def, ifdef, ifndef)
#define _BUDGIE_SWITCH2(def, ifdef, ifndef) \
    _BUDGIE_SWITCH3(_BUDGIE_SWITCH_PRE ## def(ifdef), (ifndef), _BUDGIE_SWITCH_POST ## def(ifdef))
#define _BUDGIE_SWITCH3(pre, ifndef, post) _BUDGIE_SWITCH_2ND(pre, ifndef, post)
#define _BUDGIE_SWITCH_2ND(a, b, c) b
#define _BUDGIE_SWITCH_PRE1(ifdef) 0, (ifdef), (
#define _BUDGIE_SWITCH_POST1(ifdef) )

#endif /* !BUGLE_BUDGIE_MACROS_H */
