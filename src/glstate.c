/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/utils.h"
#include "glstate.h"
#include "glutils.h"
#include "budgielib/state.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <assert.h>

static GLenum state_to_enum(state_generic *state)
{
    /* FIXME: should be made must faster (store in the specs) */
    return bugle_gl_token_to_enum(state->spec->name);
}

int glstate_compare_GLint(const void *a, const void *b)
{
    GLint A, B;

    A = *(const GLint *) a;
    B = *(const GLint *) b;
    return (A < B) ? -1 : (A > B) ? 1 : 0;
}

int glstate_compare_GLuint(const void *a, const void *b)
{
    GLuint A, B;

    A = *(const GLuint *) a;
    B = *(const GLuint *) b;
    return (A < B) ? -1 : (A > B) ? 1 : 0;
}

int glstate_compare_GLenum(const void *a, const void *b)
{
    GLenum A, B;

    A = *(const GLenum *) a;
    B = *(const GLenum *) b;
    return (A < B) ? -1 : (A > B) ? 1 : 0;
}

int glstate_compare_GLXContext(const void *a, const void *b)
{
    GLXContext A, B;

    A = *(const GLXContext *) a;
    B = *(const GLXContext *) b;
    return (A < B) ? -1 : (A > B) ? 1 : 0;
}

void glstate_get_enable(state_generic *state)
{
    GLenum e;

    e = state_to_enum(state);
    assert(state->spec->data_type == TYPE_9GLboolean);
    bugle_begin_internal_render();
    *(GLboolean *) state->data = CALL_glIsEnabled(e);
    bugle_end_internal_render("glstate_get_enable", true);
}

void glstate_get_global(state_generic *state)
{
    GLenum e;
    GLdouble data[16]; /* 16 should be enough */

    bugle_begin_internal_render();
    e = state_to_enum(state);
    switch (state->spec->data_type)
    {
    case TYPE_P6GLvoid:
        CALL_glGetPointerv(e, (GLvoid **) state->data);
         break;
    case TYPE_8GLdouble:
        CALL_glGetDoublev(e, (GLdouble *) state->data);
        break;
    case TYPE_7GLfloat:
        CALL_glGetFloatv(e, (GLfloat *) state->data);
        break;
    case TYPE_5GLint:
    case TYPE_6GLuint:
#if GLENUM_IS_GLUINT
    case TYPE_6GLenum:
#endif
        CALL_glGetIntegerv(e, (GLint *) state->data);
        break;
    case TYPE_9GLboolean:
        CALL_glGetBooleanv(e, (GLboolean *) state->data);
        break;
    default:
        assert((size_t) state->spec->data_length <= sizeof(data) / sizeof(data[0]));
        CALL_glGetDoublev(e, data);
        type_convert(state->data, state->spec->data_type, data, TYPE_8GLdouble, state->spec->data_length);
    }
    bugle_end_internal_render("glstate_get_global", true);
}

static GLenum target_to_binding(GLenum target)
{
    switch (target)
    {
    case GL_TEXTURE_1D: return GL_TEXTURE_BINDING_1D;
    case GL_TEXTURE_2D: return GL_TEXTURE_BINDING_2D;
#ifdef GL_ARB_texture_cube_map
    case GL_TEXTURE_CUBE_MAP_ARB: return GL_TEXTURE_BINDING_CUBE_MAP_ARB;
#endif
#ifdef GL_VERSION_1_2
    /* For some reason GL_TEXTURE_BINDING_3D_EXT did not exist in the
     * GL_EXT_texture3D extension
     */
    case GL_TEXTURE_3D: return GL_TEXTURE_BINDING_3D;
#endif
    default: abort();
    }
}

static GLenum get_texture_target(state_generic *state)
{
    return *(GLenum *) state->parent->key;
}

/* Assumes that the caller handles begin_internal_render */
static GLenum push_texture_binding(GLenum target, state_generic *state)
{
    GLenum binding;
    GLuint old, texture;

    binding = target_to_binding(target);
    CALL_glGetIntegerv(binding, (GLint *) &old);
    texture = *(GLuint *) state->key;
    CALL_glBindTexture(target, texture);
    return old;
}

/* Assumes that the called handles begin_internal_render */
static void pop_texture_binding(GLenum target, GLenum old)
{
    CALL_glBindTexture(target, old);
}

/* Assumes that the called handles begin_internal_render */
static GLenum push_server_texture_unit(state_generic *state)
{
    GLenum old, cur;

    /* FIXME: check if we really have multitex */
    CALL_glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, (GLint *) &old);
    cur = *(GLenum *) state->key;
    if (cur != old)
    {
        CALL_glActiveTextureARB(cur);
        return old;
    }
    else
        return 0;
}

/* Assumes that the called handles begin_internal_render */
static inline void pop_server_texture_unit(GLenum old)
{
    if (old)
        CALL_glActiveTextureARB(old);
}

void glstate_get_texparameter(state_generic *state)
{
    GLenum old_texture;
    GLenum target;
    GLenum e;
    float data[16]; /* should be enough */

    bugle_begin_internal_render();
    target = get_texture_target(state->parent);
    old_texture = push_texture_binding(target, state->parent);

    e = state_to_enum(state);
    switch (state->spec->data_type)
    {
    case TYPE_5GLint:
    case TYPE_6GLuint:
#if GLENUM_IS_GLUINT
    case TYPE_6GLenum:
#endif
        (*glGetTexParameteriv)(target, e, state->data);
        break;
    case TYPE_7GLfloat:
        (*glGetTexParameterfv)(target, e, state->data);
        break;
    default:
        assert((size_t) state->spec->data_length <= sizeof(data) / sizeof(data[0]));
        (*glGetTexParameterfv)(target, e, data);
        type_convert(state->data, state->spec->data_type, data, TYPE_7GLfloat, state->spec->data_length);
    }

    pop_texture_binding(target, old_texture);
    bugle_end_internal_render("glstate_get_texparameter", true);
}

void glstate_get_texlevelparameter(state_generic *state)
{
    state_generic *tex_state;
    GLenum target, old_texture, e;
    GLint level;
    GLfloat data[16];

    bugle_begin_internal_render();
    tex_state = state->parent->parent->parent;
    target = get_texture_target(tex_state);
    old_texture = push_texture_binding(target, tex_state);

    e = state_to_enum(state);
    level = *(GLint *) state->parent->key;
    switch (state->spec->data_type)
    {
    case TYPE_5GLint:
    case TYPE_6GLuint:
#if GLENUM_IS_GLUINT
    case TYPE_6GLenum:
#endif
        CALL_glGetTexLevelParameteriv(target, level, e, state->data);
        break;
    case TYPE_7GLfloat:
        CALL_glGetTexLevelParameterfv(target, level, e, state->data);
        break;
    default:
        assert((size_t) state->spec->data_length <= sizeof(data) / sizeof(data[0]));
        CALL_glGetTexLevelParameterfv(target, level, e, data);
        type_convert(state->data, state->spec->data_type, data, TYPE_7GLfloat, state->spec->data_length);
    }

    pop_texture_binding(target, old_texture);
    bugle_end_internal_render("glstate_get_texlevelparameter", true);
}

void glstate_get_texgen(state_generic *state)
{
    GLenum old_unit;
    GLenum coord;
    GLenum e;
    GLdouble data[16];

    bugle_begin_internal_render();
    old_unit = push_server_texture_unit(state->parent->parent->parent);
    coord = *(GLenum *) state->parent->key;
    if (state->spec->data_type == TYPE_9GLboolean) /* enable bit */
        *(GLboolean *) state->data = CALL_glIsEnabled(coord);
    else
    {
        e = state_to_enum(state);
        switch (state->spec->data_type)
        {
        case TYPE_8GLdouble:
            CALL_glGetTexGendv(coord, e, (GLdouble *) state->data);
            break;
        case TYPE_7GLfloat:
            CALL_glGetTexGenfv(coord, e, (GLfloat *) state->data);
            break;
        case TYPE_5GLint:
        case TYPE_6GLuint:
#if GLENUM_IS_GLUINT
        case TYPE_6GLenum:
#endif
            CALL_glGetTexGeniv(coord, e, (GLint *) state->data);
            break;
        default:
            assert((size_t) state->spec->data_length <= sizeof(data) / sizeof(data[0]));
            CALL_glGetTexGendv(coord, e, data);
            type_convert(state->data, state->spec->data_type, data, TYPE_8GLdouble, state->spec->data_length);
        }
    }

    pop_server_texture_unit(old_unit);
    bugle_end_internal_render("glstate_get_texgen", true);
}

void glstate_get_texunit(state_generic *state)
{
    GLenum old_unit;
    GLenum e;
    GLdouble data[16];

    bugle_begin_internal_render();
    old_unit = push_server_texture_unit(state->parent);
    e = state_to_enum(state);
    if (state->spec->data_type == TYPE_9GLboolean) /* enable bit */
        *(GLboolean *) state->data = CALL_glIsEnabled(e);
    else
    {
        switch (state->spec->data_type)
        {
        case TYPE_8GLdouble:
            CALL_glGetDoublev(e, (GLdouble *) state->data);
            break;
        case TYPE_7GLfloat:
            CALL_glGetFloatv(e, (GLfloat *) state->data);
            break;
        case TYPE_5GLint:
        case TYPE_6GLuint:
#if GLENUM_IS_GLUINT
        case TYPE_6GLenum:
#endif
            CALL_glGetIntegerv(e, (GLint *) state->data);
            break;
        default:
            assert((size_t) state->spec->data_length <= sizeof(data) / sizeof(data[0]));
            CALL_glGetDoublev(e, data);
            type_convert(state->data, state->spec->data_type, data, TYPE_8GLdouble, state->spec->data_length);
        }
    }

    pop_server_texture_unit(old_unit);
    bugle_end_internal_render("glstate_get_texunit", true);
}

static void glstate_get_texenv(state_generic *state, GLenum target)
{
    GLenum old_unit, e;
    GLfloat data[16];

    bugle_begin_internal_render();
    old_unit = push_server_texture_unit(state->parent);

    e = state_to_enum(state);
    switch (state->spec->data_type)
    {
    case TYPE_7GLfloat:
        CALL_glGetTexEnvfv(target, e, state->data);
        break;
    case TYPE_5GLint:
    case TYPE_6GLuint:
#if GLENUM_IS_GLUINT
    case TYPE_6GLenum:
#endif
        CALL_glGetTexEnviv(target, e, state->data);
        break;
    default:
        assert((size_t) state->spec->data_length <= sizeof(data) / sizeof(data[0]));
        CALL_glGetTexEnvfv(target, e, data);
        type_convert(state->data, state->spec->data_type, data, TYPE_7GLfloat, state->spec->data_length);
    }

    pop_server_texture_unit(old_unit);
    bugle_end_internal_render("glstate_get_texenv", true);
}

void glstate_get_textureenv(state_generic *state)
{
    glstate_get_texenv(state, GL_TEXTURE_ENV);
}

void glstate_get_texturefiltercontrol(state_generic *state)
{
#ifdef GL_VERSION_1_4
    glstate_get_texenv(state, GL_TEXTURE_FILTER_CONTROL);
#endif
}

void glstate_get_light(state_generic *state)
{
    GLenum e, light;
    GLfloat data[16];

    bugle_begin_internal_render();
    e = state_to_enum(state);
    light = *(GLenum *) state->parent->key;
    if (state->spec->data_type == TYPE_9GLboolean) /* enable bit */
        *(GLboolean *) state->data = CALL_glIsEnabled(light);
    else
        switch (state->spec->data_type)
        {
        case TYPE_7GLfloat:
            CALL_glGetLightfv(light, e, state->data);
            break;
        case TYPE_5GLint:
        case TYPE_6GLuint:
#if GLENUM_IS_GLUINT
        case TYPE_6GLenum:
#endif
            CALL_glGetLightiv(light, e, state->data);
            break;
        default:
            assert((size_t) state->spec->data_length <= sizeof(data) / sizeof(data[0]));
            CALL_glGetLightfv(light, e, data);
            type_convert(state->data, state->spec->data_type, data, TYPE_7GLfloat, state->spec->data_length);
        }

    bugle_end_internal_render("glstate_get_light", true);
}
