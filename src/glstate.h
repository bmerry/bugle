/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef BUGLE_SRC_GLSTATE_H
#define BUGLE_SRC_GLSTATE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>
#include <GL/gl.h>
#include "src/tracker.h"         /* For gl_handle */
#include "common/linkedlist.h"

typedef struct state_info
{
    const char *name;
    GLenum pname;
    budgie_type type;
    int length;
    int extension;
    const char *version;
    unsigned int flags;
} state_info;

typedef struct glstate
{
    char *name;

    /* context */
    GLenum target, face, binding;
    GLenum unit;
    gl_handle object;
    GLint level;
    const struct state_info *info;

    void (*spawn_children)(const struct glstate *, bugle_linked_list *);
} glstate;

/* Must be in a valid state to make GL calls.
 * Must also have trackextensions and trackobjects.
 */
char *bugle_state_get_string(const glstate *); /* caller frees */
void bugle_state_get_children(const glstate *, bugle_linked_list *);
void bugle_state_clear(glstate *);
const glstate *bugle_state_get_root(void);

extern const state_info * const all_state[];

#endif /* !BUGLE_SRC_GLSTATE_H */
