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

#ifndef BUGLE_BUDGIE_BASICTYPES_H
#define BUGLE_BUDGIE_BASICTYPES_H


typedef int budgie_function;
typedef int budgie_group;
typedef int budgie_type;

#define NULL_FUNCTION (-1)
#define NULL_GROUP (-1)
#define NULL_TYPE (-1)

#define BUDGIE_MAX_ARGS 32

/* If you modify this, be sure to update write_call_structs to set up the
 * function_call union with compatible memory layout.
 */
typedef struct
{
    budgie_group group;
    budgie_function id;
    int num_args;
    void *user_data;
    void *retn;
    void *args[BUDGIE_MAX_ARGS];
} generic_function_call;

#endif /* !BUGLE_BUDGIE_BASICTYPES_H */
