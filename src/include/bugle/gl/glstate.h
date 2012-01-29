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

#ifndef BUGLE_SRC_GLSTATE_H
#define BUGLE_SRC_GLSTATE_H

#include <stddef.h>
#include <bugle/gl/glheaders.h>
#include <bugle/linkedlist.h>
#include <bugle/porting.h>

#ifdef __cplusplus
extern "C" {
#endif

struct glstate;

typedef struct state_info
{
    const char *name;
    GLenum pname;
    budgie_type type;
    int length;
    int extensions;
    int exclude;
    unsigned int flags;
    /* Optional: arguments are the parent, the children list, and the state_info
     * that contained the callback. If present, the other fields are ignored
     * and can be used to pass information to the callback. However,
     * extensions and exclude are still tested to determine whether to
     * invoke the callback.
     */
    void (*spawn)(const struct glstate *, linked_list *, const struct state_info *);
} state_info;

typedef struct glstate
{
    char *name;
    GLint numeric_name;
    GLenum enum_name;

    /* context */
    GLenum target, face, binding;
    GLenum unit;
    GLuint object;
    GLint level;
#if BUGLE_GLTYPE_GL
    GLsync sync;   /* Special case of object for sync objects, which aren't GLuints */
#endif
    const struct state_info *info;

    void (*spawn_children)(const struct glstate *, linked_list *);
} glstate;

/* A structure used to return values from bugle_state_get_raw. data
 * is a pointer to a binary blob. type is the base type of the data, while
 * length is the number of elements of type (-1 if it is not an array).
 * The caller is responsible for freeing the data. If data is NULL then the
 * data could not be queried.
 *
 * In the case of a string, the type is TYPE_Pc (even if from an OpenGL
 * function that returns GLchar or GLubyte) and the length is strlen(data).
 */
typedef struct
{
    void *data;
    budgie_type type;
    int length;
} bugle_state_raw;

/* Must be in a valid state to make GL calls.
 * Must also have glextensions and globjects.
 */
BUGLE_EXPORT_PRE void bugle_state_get_raw(const glstate *, bugle_state_raw *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE char *bugle_state_get_string(const glstate *) BUGLE_EXPORT_POST; /* caller frees */
BUGLE_EXPORT_PRE void bugle_state_get_children(const glstate *, linked_list *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_state_clear(glstate *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE const glstate *bugle_state_get_root(void) BUGLE_EXPORT_POST;

extern const state_info * const all_state[];

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_SRC_GLSTATE_H */
