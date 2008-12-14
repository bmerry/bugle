/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define GL_GLEXT_PROTOTYPES
#include <bugle/gl/glheaders.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <inttypes.h>
#include "xalloc.h"
#include "xvasprintf.h"
#include <bugle/log.h>
#include <bugle/misc.h>
#include <bugle/gl/glsl.h>
#include <bugle/gl/glstate.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/globjects.h>
#include <bugle/gl/glextensions.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include <budgie/call.h>
#include "budgielib/defines.h"
#include "src/apitables.h"

#define STATE_NAME(x) #x, x
#define STATE_NAME_EXT(x, ext) #x, x ## ext

/* Flags are divided up as follows:
 * 0-7: fetch function indicator
 * 8-15: multiplexor state
 * 16-27: conditional selection flags
 * 28-31: other
 */
#define STATE_MODE_GLOBAL                   0x00000001    /* glGet* */
#define STATE_MODE_ENABLED                  0x00000002    /* glIsEnabled */
#define STATE_MODE_TEXTURE_ENV              0x00000003    /* glGetTexEnv(GL_TEXTURE_ENV, ...) */
#define STATE_MODE_POINT_SPRITE             0x00000005    /* glGetTexEnv(GL_POINT_SPRITE, ...) */
#define STATE_MODE_TEX_PARAMETER            0x00000006    /* glGetTexParameter */
#define STATE_MODE_TEX_GEN                  0x00000008    /* glGetTexGen */
#define STATE_MODE_LIGHT                    0x00000009    /* glGetLight */
#define STATE_MODE_MATERIAL                 0x0000000a    /* glGetMaterial */
#define STATE_MODE_CLIP_PLANE               0x0000000b    /* glGetClipPlane */
#define STATE_MODE_BUFFER_PARAMETER         0x00000014    /* glGetBufferParameter */
#define STATE_MODE_COMPRESSED_TEXTURE_FORMATS 0x00000020  /* glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, ...) */
#define STATE_MODE_RENDERBUFFER_PARAMETER   0x00000021    /* glGetRenderbufferParameterivEXT */
#define STATE_MODE_FRAMEBUFFER_ATTACHMENT_PARAMETER 0x00000022 /* glGetFramebufferAttachmentPArameterivEXT */
#define STATE_MODE_MASK                     0x000000ff

#define STATE_MULTIPLEX_ACTIVE_TEXTURE      0x00000100    /* Set active texture */
#define STATE_MULTIPLEX_BIND_TEXTURE        0x00000200    /* Set current texture object */
#define STATE_MULTIPLEX_BIND_BUFFER         0x00000400    /* Set current buffer object (GL_ARRAY_BUFFER) */
#define STATE_MULTIPLEX_BIND_FRAMEBUFFER    0x00001000    /* Set current framebuffer object */
#define STATE_MULTIPLEX_BIND_RENDERBUFFER   0x00002000    /* Set current renderbuffer object */

#define STATE_SELECT_NO_2D                  0x00020000    /* Ignore for 2D targets like GL_TEXTURE_2D */
#define STATE_SELECT_NON_ZERO               0x00040000    /* Ignore if some field is 0 */
#define STATE_SELECT_COMPRESSED             0x00100000    /* Only applies for compressed textures */
#define STATE_SELECT_TEXTURE_ENV            0x00800000    /* Texture unit state that applies to texture application (limited to GL_MAX_TEXTURE_UNITS) */
#define STATE_SELECT_TEXTURE_COORD          0x01000000    /* Texture unit state that applies to texture coordinates (limited to GL_MAX_TEXTURE_COORDS) */
#define STATE_SELECT_TEXTURE_IMAGE          0x02000000    /* Texture unit state that applies to texture images (limited to GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) */

/* conveniences */
#define STATE_GLOBAL STATE_MODE_GLOBAL
#define STATE_ENABLED STATE_MODE_ENABLED
#define STATE_TEXTURE_ENV (STATE_MODE_TEXTURE_ENV | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_POINT_SPRITE (STATE_MODE_POINT_SPRITE | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_TEX_UNIT (STATE_MODE_GLOBAL | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_TEX_UNIT_ENABLED (STATE_MODE_ENABLED | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_TEX_PARAMETER (STATE_MODE_TEX_PARAMETER | STATE_MULTIPLEX_BIND_TEXTURE)
#define STATE_TEX_GEN (STATE_MODE_TEX_GEN | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_LIGHT STATE_MODE_LIGHT
#define STATE_MATERIAL STATE_MODE_MATERIAL
#define STATE_CLIP_PLANE STATE_MODE_CLIP_PLANE
#define STATE_BUFFER_PARAMETER (STATE_MODE_BUFFER_PARAMETER | STATE_MULTIPLEX_BIND_BUFFER)
#define STATE_COMPRESSED_TEXTURE_FORMATS STATE_MODE_COMPRESSED_TEXTURE_FORMATS
#define STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER (STATE_MODE_FRAMEBUFFER_ATTACHMENT_PARAMETER | STATE_MULTIPLEX_BIND_FRAMEBUFFER)
#define STATE_FRAMEBUFFER_PARAMETER (STATE_MODE_GLOBAL | STATE_MULTIPLEX_BIND_FRAMEBUFFER)
#define STATE_RENDERBUFFER_PARAMETER (STATE_MODE_RENDERBUFFER_PARAMETER | STATE_MULTIPLEX_BIND_RENDERBUFFER)

typedef struct
{
    const char *name;
    GLenum token;
    int extensions;
} enum_list;

static const enum_list material_enums[] =
{
    { STATE_NAME(GL_FRONT), BUGLE_GL_VERSION_ES_CM_1_0 },
    { STATE_NAME(GL_BACK), BUGLE_GL_VERSION_ES_CM_1_0 },
    { NULL, GL_NONE, 0 }
};

static const enum_list cube_map_face_enums[] =
{
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES), BUGLE_GL_OES_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_NEGATIVE_X_OES), BUGLE_GL_OES_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_POSITIVE_Y_OES), BUGLE_GL_OES_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_OES), BUGLE_GL_OES_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_POSITIVE_Z_OES), BUGLE_GL_OES_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_OES), BUGLE_GL_OES_texture_cube_map },
    { NULL, GL_NONE, 0 }
};

static const state_info material_state[] =
{
    { STATE_NAME(GL_AMBIENT), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_MATERIAL },
    { STATE_NAME(GL_DIFFUSE), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_MATERIAL },
    { STATE_NAME(GL_SPECULAR), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_MATERIAL },
    { STATE_NAME(GL_EMISSION), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_MATERIAL },
    { STATE_NAME(GL_SHININESS), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_MATERIAL },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info light_state[] =
{
    { STATE_NAME(GL_AMBIENT), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { STATE_NAME(GL_DIFFUSE), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { STATE_NAME(GL_SPECULAR), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { STATE_NAME(GL_POSITION), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { STATE_NAME(GL_CONSTANT_ATTENUATION), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { STATE_NAME(GL_LINEAR_ATTENUATION), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { STATE_NAME(GL_QUADRATIC_ATTENUATION), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { STATE_NAME(GL_SPOT_DIRECTION), TYPE_7GLfloat, 3, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { STATE_NAME(GL_SPOT_EXPONENT), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { STATE_NAME(GL_SPOT_CUTOFF), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_LIGHT },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_unit_state[] =
{
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_COORD_REPLACE_OES), TYPE_9GLboolean, -1, BUGLE_GL_OES_point_sprite, -1, STATE_POINT_SPRITE | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_ENV_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_ENV_COLOR), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV  },
    { STATE_NAME(GL_COMBINE_RGB), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_COMBINE_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SRC0_RGB), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SRC1_RGB), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SRC2_RGB), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SRC0_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SRC1_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SRC2_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND0_RGB), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND1_RGB), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND2_RGB), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND0_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND1_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND2_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_RGB_SCALE), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_ALPHA_SCALE), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_CURRENT_TEXTURE_COORDS), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_MATRIX), TYPE_7GLfloat, 16, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_2D), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_OES), TYPE_9GLboolean, -1, BUGLE_GL_OES_texture_cube_map, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_BINDING_2D), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_CUBE_MAP_OES), TYPE_5GLint, -1, BUGLE_GL_OES_texture_cube_map, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_parameter_state[] =
{
    { STATE_NAME(GL_TEXTURE_MIN_FILTER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_MAG_FILTER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_S), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_T), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_GENERATE_MIPMAP), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_TEX_PARAMETER },

#ifdef GL_EXT_texture_filter_anisotropic
    { STATE_NAME(GL_TEXTURE_MAX_ANISOTROPY_EXT), TYPE_7GLfloat, -1, BUGLE_GL_EXT_texture_filter_anisotropic, -1, STATE_TEX_PARAMETER },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info generic_enable = { NULL, GL_NONE, TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED };

/* All the make functions _append_ nodes to children. That means you have
 * to initialise the list yourself.
 */
static void make_leaves_conditional(const glstate *self, const state_info *table,
                                    uint32_t flags, unsigned int mask,
                                    linked_list *children)
{
    const char *version;
    glstate *child;
    const state_info *info;

    version = (const char *) CALL(glGetString)(GL_VERSION);
    for (info = table; info->name; info++)
    {
        if ((info->flags & mask) == flags
            && (bugle_gl_has_extension_group(info->extensions))
            && (info->exclude == -1 || !bugle_gl_has_extension_group(info->exclude)))
        {
            if (info->spawn)
                info->spawn(self, children, info);
            else
            {
                child = XMALLOC(glstate);
                *child = *self; /* copies contextual info */
                child->name = xstrdup(info->name);
                child->numeric_name = 0;
                child->enum_name = info->pname;
                child->info = info;
                child->spawn_children = NULL;
                bugle_list_append(children, child);
            }
        }
    }
}

static void make_leaves(const glstate *self, const state_info *table,
                        linked_list *children)
{
    make_leaves_conditional(self, table, 0, 0, children);
}

static void make_fixed(const glstate *self,
                       const enum_list *enums,
                       size_t offset,
                       void (*spawn)(const glstate *, linked_list *),
                       const state_info *info,
                       linked_list *children)
{
    size_t i;
    glstate *child;

    for (i = 0; enums[i].name; i++)
        if (bugle_gl_has_extension_group(enums[i].extensions))
        {
            child = XMALLOC(glstate);
            *child = *self;
            child->info = info;
            child->name = xstrdup(enums[i].name);
            child->numeric_name = 0;
            child->enum_name = enums[i].token;
            *(GLenum *) (((char *) child) + offset) = enums[i].token;
            child->spawn_children = spawn;
            bugle_list_append(children, child);
        }
}

static void make_counted2(const glstate *self,
                          GLint count,
                          const char *format,
                          GLenum base,
                          size_t offset1, size_t offset2,
                          void (*spawn)(const glstate *, linked_list *),
                          const state_info *info,
                          linked_list *children)
{
    GLint i;
    glstate *child;

    for (i = 0; i < count; i++)
    {
        child = XMALLOC(glstate);
        *child = *self;
        child->info = info;
        child->name = xasprintf(format, (unsigned long) i);
        child->numeric_name = i;
        child->enum_name = 0;
        *(GLenum *) (((char *) child) + offset1) = base + i;
        *(GLenum *) (((char *) child) + offset2) = base + i;
        child->spawn_children = spawn;
        bugle_list_append(children, child);
    }
}

static void make_object(const glstate *self,
                        GLenum target,
                        const char *format,
                        GLuint id,
                        void (*spawn)(const glstate *, linked_list *),
                        const state_info *info,
                        linked_list *children)
{
    glstate *child;

    child = XMALLOC(glstate);
    *child = *self;
    child->target = target;
    child->info = info;
    child->name = xasprintf(format, (unsigned long) id);
    child->numeric_name = id;
    child->enum_name = 0;
    child->object = id;
    child->spawn_children = spawn;
    bugle_list_append(children, child);
}

typedef struct
{
    const glstate *self;
    GLenum target;
    const char *format;
    void (*spawn_children)(const glstate *, linked_list *);
    state_info *info;
    linked_list *children;
} make_objects_data;

static void make_objects_walker(GLuint object,
                                GLenum target,
                                void *vdata)
{
    const make_objects_data *data;

    data = (const make_objects_data *) vdata;
    if (data->target != GL_NONE && target != GL_NONE
        && target != data->target) return;
    make_object(data->self, target, data->format, object, data->spawn_children,
                data->info, data->children);
}

static void make_objects(const glstate *self,
                         bugle_globjects_type type,
                         GLenum target,
                         bool add_zero,
                         const char *format,
                         void (*spawn_children)(const glstate *, linked_list *),
                         state_info *info,
                         linked_list *children)
{
    make_objects_data data;

    data.self = self;
    data.target = target;
    data.format = format;
    data.spawn_children = spawn_children;
    data.info = info;
    data.children = children;

    if (add_zero)
        make_object(self, target, format, 0, spawn_children, info, children);
    bugle_globjects_walk(type, make_objects_walker, &data);
}

static void make_target(const glstate *self,
                        const char *name,
                        GLenum target,
                        GLenum binding,
                        void (*spawn_children)(const glstate *, linked_list *),
                        const state_info *info,
                        linked_list *children)
{
    glstate *child;

    child = XMALLOC(glstate);
    *child = *self;
    child->name = xstrdup(name);
    child->numeric_name = 0;
    child->enum_name = target;
    child->target = target;
    child->face = target;   /* Changed at next level for cube maps */
    child->binding = binding;
    child->spawn_children = spawn_children;
    child->info = info;
    bugle_list_append(children, child);
}

static void spawn_clip_planes(const struct glstate *self,
                              linked_list *children,
                              const struct state_info *info)
{
    GLint count;
    CALL(glGetIntegerv)(GL_MAX_CLIP_PLANES, &count);
    make_counted2(self, count, "GL_CLIP_PLANE%lu", GL_CLIP_PLANE0,
                  offsetof(glstate, target), offsetof(glstate, enum_name),
                  NULL, info, children);
}

static void spawn_children_material(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, material_state, children);
}

static void spawn_materials(const struct glstate *self,
                            linked_list *children,
                            const struct state_info *info)
{
    make_fixed(self, material_enums, offsetof(glstate, target),
               spawn_children_material, NULL, children);
}

static void spawn_children_light(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, light_state, children);
}

static void spawn_lights(const glstate *self,
                         linked_list *children,
                         const struct state_info *info)
{
    GLint count;

    CALL(glGetIntegerv)(GL_MAX_LIGHTS, &count);
    make_counted2(self, count, "GL_LIGHT%lu", GL_LIGHT0,
                  offsetof(glstate, target), offsetof(glstate, enum_name),
                  spawn_children_light,
                  &generic_enable, children);
}

static int get_total_texture_units(void)
{
    GLint max = 1;

    CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &max);
    return max;
}

/* Returns a mask of flags not to select from state tables, based on the
 * dimension of the target.
 */
static uint32_t texture_mask(GLenum target)
{
    uint32_t mask = 0;

    switch (target)
    {
    case GL_TEXTURE_2D: mask |= STATE_SELECT_NO_2D; break;
    case GL_TEXTURE_CUBE_MAP_OES: mask |= STATE_SELECT_NO_2D; break;
    }
    return mask;
}

static unsigned int get_texture_env_units(void)
{
    return get_total_texture_units();
}

static unsigned int get_texture_image_units(void)
{
    return get_total_texture_units();
}

static unsigned int get_texture_coord_units(void)
{
    return get_total_texture_units();
}

static void spawn_children_tex_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    if (self->binding) /* Zero indicates a proxy, which have no texture parameter state */
        make_leaves_conditional(self, tex_parameter_state, 0,
                                texture_mask(self->target), children);
}

static void spawn_children_tex_target(const glstate *self, linked_list *children)
{
    if (self->binding) /* Zero here indicates a proxy target */
    {
        bugle_list_init(children, free);
        make_objects(self, BUGLE_GLOBJECTS_TEXTURE, self->target, true, "%lu",
                     spawn_children_tex_parameter, NULL, children);
    }
    else
        spawn_children_tex_parameter(self, children);
}

static void spawn_children_tex_unit(const glstate *self, linked_list *children)
{
    linked_list_node *i;
    glstate *child;
    uint32_t mask = 0;

    bugle_list_init(children, free);
    if (self->unit >= GL_TEXTURE0 + get_texture_env_units())
        mask |= STATE_SELECT_TEXTURE_ENV;
    if (self->unit >= GL_TEXTURE0 + get_texture_coord_units())
        mask |= STATE_SELECT_TEXTURE_COORD;
    if (self->unit >= GL_TEXTURE0 + get_texture_image_units())
        mask |= STATE_SELECT_TEXTURE_IMAGE;
    make_leaves_conditional(self, tex_unit_state, 0, mask, children);
    for (i = bugle_list_head(children); i; i = bugle_list_next(i))
    {
        child = (glstate *) bugle_list_data(i);
        switch (child->info->flags & STATE_MODE_MASK)
        {
        case STATE_MODE_TEXTURE_ENV:
            child->target = GL_TEXTURE_ENV;
            break;
        case STATE_MODE_POINT_SPRITE:
            child->target = GL_POINT_SPRITE_OES;
            break;
        }
    }
}

static void spawn_texture_units(const glstate *self,
                                linked_list *children,
                                const struct state_info *info)
{
    GLint count;
    count = get_total_texture_units();
    make_counted2(self, count, "GL_TEXTURE%lu", GL_TEXTURE0,
                  offsetof(glstate, unit), offsetof(glstate, enum_name),
                  spawn_children_tex_unit,
                  NULL, children);
}

static void spawn_textures(const glstate *self,
                           linked_list *children,
                           const struct state_info *info)
{
    make_target(self, "GL_TEXTURE_2D", GL_TEXTURE_2D,
                GL_TEXTURE_BINDING_2D, spawn_children_tex_target, NULL, children);
    if (bugle_gl_has_extension_group(BUGLE_GL_OES_texture_cube_map))
    {
        make_target(self, "GL_TEXTURE_CUBE_MAP_OES",
                    GL_TEXTURE_CUBE_MAP_OES,
                    GL_TEXTURE_BINDING_CUBE_MAP_OES,
                    spawn_children_tex_target, NULL, children);
    }
}

/*** Main state table ***/

static const state_info global_state[] =
{
    { STATE_NAME(GL_CURRENT_COLOR), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_NORMAL), TYPE_7GLfloat, 3, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CLIENT_ACTIVE_TEXTURE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_VERTEX_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_NORMAL_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ELEMENT_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    /* FIXME: these are matrix stacks, not just single matrices */
    { STATE_NAME(GL_MODELVIEW_MATRIX), TYPE_7GLfloat, 16, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PROJECTION_MATRIX), TYPE_7GLfloat, 16, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VIEWPORT), TYPE_5GLint, 4, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_RANGE), TYPE_7GLfloat, 2, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MODELVIEW_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PROJECTION_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MATRIX_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMALIZE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_RESCALE_NORMAL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    /* FIXME: clip plane enables */
    { STATE_NAME(GL_CLIP_PLANE0), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_CLIP_PLANE, spawn_clip_planes },
    { STATE_NAME(GL_FOG_COLOR), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_DENSITY), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_START), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_END), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SHADE_MODEL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIGHTING), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_MATERIAL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { "Materials", 0, NULL_TYPE, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, 0, spawn_materials },
    { STATE_NAME(GL_LIGHT_MODEL_AMBIENT), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_TWO_SIDE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { "Lights", 0, NULL_TYPE, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, 0, spawn_lights },
    { STATE_NAME(GL_POINT_SIZE), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SMOOTH), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_POINT_SPRITE_OES), TYPE_9GLboolean, -1, BUGLE_GL_OES_point_sprite, -1, STATE_ENABLED },
    { STATE_NAME(GL_POINT_SIZE_MIN), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SIZE_MAX), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_FADE_THRESHOLD_SIZE), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_DISTANCE_ATTENUATION), TYPE_7GLfloat, 3, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LINE_WIDTH), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LINE_SMOOTH), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_CULL_FACE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_CULL_FACE_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRONT_FACE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_FACTOR), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_UNITS), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_FILL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MULTISAMPLE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_ALPHA_TO_COVERAGE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_ALPHA_TO_ONE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_COVERAGE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_COVERAGE_VALUE), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLE_COVERAGE_INVERT), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { "Texture units", GL_TEXTURE0, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_texture_units },
    { "Textures", GL_TEXTURE_2D, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_textures },
    { STATE_NAME(GL_ACTIVE_TEXTURE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SCISSOR_TEST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SCISSOR_BOX), TYPE_5GLint, 4, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_TEST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_ALPHA_TEST_FUNC), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_TEST_REF), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_TEST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_STENCIL_FUNC), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_VALUE_MASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_REF), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_FAIL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_PASS_DEPTH_FAIL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_PASS_DEPTH_PASS), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_TEST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_DEPTH_FUNC), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_BLEND_SRC_RGB_OES), TYPE_11GLblendenum, -1, BUGLE_GL_OES_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_SRC_ALPHA_OES), TYPE_11GLblendenum, -1, BUGLE_GL_OES_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_DST_RGB_OES), TYPE_11GLblendenum, -1, BUGLE_GL_OES_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_DST_ALPHA_OES), TYPE_11GLblendenum, -1, BUGLE_GL_OES_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_SRC), TYPE_11GLblendenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, BUGLE_GL_OES_blend_func_separate, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_DST), TYPE_11GLblendenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, BUGLE_GL_OES_blend_func_separate, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_EQUATION_RGB_OES), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_EQUATION_ALPHA_OES), TYPE_6GLenum, -1, BUGLE_GL_OES_blend_equation_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DITHER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_WRITEMASK), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_WRITEMASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_CLEAR_VALUE), TYPE_7GLfloat, 4, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_CLEAR_VALUE), TYPE_7GLfloat, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_CLEAR_VALUE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },

    { STATE_NAME(GL_PERSPECTIVE_CORRECTION_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SMOOTH_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LINE_SMOOTH_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_GENERATE_MIPMAP_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_LIGHTS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CLIP_PLANES), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_MODELVIEW_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROJECTION_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SUBPIXEL_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CUBE_MAP_TEXTURE_SIZE_OES), TYPE_5GLint, -1, BUGLE_GL_OES_texture_cube_map, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VIEWPORT_DIMS), TYPE_5GLint, 2, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_POINT_SIZE_RANGE), TYPE_7GLfloat, 2, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_POINT_SIZE_RANGE), TYPE_7GLfloat, 2, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_LINE_WIDTH_RANGE), TYPE_7GLfloat, 2, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_LINE_WIDTH_RANGE), TYPE_7GLfloat, 2, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLE_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COMPRESSED_TEXTURE_FORMATS), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_COMPRESSED_TEXTURE_FORMATS },
    { STATE_NAME(GL_NUM_COMPRESSED_TEXTURE_FORMATS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EXTENSIONS), TYPE_PKc, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RENDERER), TYPE_PKc, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VENDOR), TYPE_PKc, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERSION), TYPE_PKc, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_UNITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_0, -1, STATE_GLOBAL },
    /* FIXME: glGetError */

#ifdef GL_OES_texture_filter_anisotropic
    { STATE_NAME(GL_MAX_TEXTURE_MAX_ANISOTROPY_OES), TYPE_7GLfloat, -1, BUGLE_GL_OES_texture_filter_anisotropic, -1, STATE_GLOBAL },
#endif
#ifdef GL_OES_framebuffer_object
    { STATE_NAME(GL_RENDERBUFFER_BINDING_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRAMEBUFFER_BINDING_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_RENDERBUFFER_SIZE_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_GLOBAL },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};


static const state_info buffer_parameter_state[] =
{
    { STATE_NAME(GL_BUFFER_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_USAGE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_BUFFER_PARAMETER },
#ifdef GL_OES_mapbuffer
    { STATE_NAME(GL_BUFFER_ACCESS_OES), TYPE_6GLenum, -1, BUGLE_GL_OES_mapbuffer, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_MAPPED_OES), TYPE_9GLboolean, -1, BUGLE_GL_OES_mapbuffer, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_MAP_POINTER_OES), TYPE_P6GLvoid, -1, BUGLE_GL_OES_mapbuffer, -1, STATE_BUFFER_PARAMETER },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info renderbuffer_parameter_state[] =
{
#ifdef GL_OES_framebuffer_object
    { STATE_NAME(GL_RENDERBUFFER_WIDTH_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_HEIGHT_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_INTERNAL_FORMAT_OES), TYPE_6GLenum, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_RED_SIZE_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_GREEN_SIZE_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_BLUE_SIZE_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_ALPHA_SIZE_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_DEPTH_SIZE_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_STENCIL_SIZE_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

/* Note: the below is used only if FBO is already available so we don't
 * have to explicitly check for it.
 */
static const state_info framebuffer_parameter_state[] =
{
#ifdef GL_OES_framebuffer_object
    { STATE_NAME(GL_SAMPLE_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ALPHA_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_DEPTH_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_STENCIL_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_STENCIL_REF), TYPE_5GLint, -1, BUGLE_GL_VERSION_ES_CM_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
#endif /* GL_OES_framebuffer_object */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info framebuffer_attachment_parameter_state[] =
{
#ifdef GL_OES_framebuffer_object
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_OES), TYPE_6GLenum, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_OES), TYPE_5GLint, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_OES), TYPE_6GLenum, -1, BUGLE_GL_OES_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
#endif /* GL_OES_framebuffer_object */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

/* This exists to simplify the work of gldump.c */
const state_info * const all_state[] =
{
    global_state,
    tex_parameter_state,
    tex_unit_state,
    light_state,
    material_state,
    buffer_parameter_state,
    framebuffer_attachment_parameter_state,
    framebuffer_parameter_state,
    renderbuffer_parameter_state,
    NULL
};

static void get_helper(const glstate *state,
                       GLfloat *f, GLint *i,
                       budgie_type *in_type,
                       void (BUDGIEAPI *get_float)(GLenum, GLenum, GLfloat *),
                       void (BUDGIEAPI *get_int)(GLenum, GLenum, GLint *))
{
    if ((state->info->type == TYPE_7GLfloat || state->info->type == TYPE_7GLfloat)
        && get_float != NULL)
    {
        get_float(state->target, state->info->pname, f);
        *in_type = TYPE_7GLfloat;
    }
    else
    {
        get_int(state->target, state->info->pname, i);
        *in_type = TYPE_5GLint;
    }
}

void bugle_state_get_raw(const glstate *state, bugle_state_raw *wrapper)
{
    GLenum error;
    GLfloat f[16];
    GLint i[16];
    GLuint ui[16];
    GLboolean b[16];
    GLvoid *p[16];
    void *in;
    budgie_type in_type, out_type;
    int in_length;
    char *str = NULL;  /* Set to non-NULL for human-readable string output */

    GLint old_texture, old_buffer;
    GLint old_unit, old_client_unit, old_framebuffer, old_renderbuffer;
    bool flag_active_texture = false;
    GLenum pname;

    if (!state->info) return;
    if (state->info->pname) pname = state->info->pname;
    else if (state->enum_name) pname = state->enum_name;
    else pname = state->target;
    in_type = state->info->type;
    in_length = state->info->length;
    out_type = state->info->type;
    wrapper->data = NULL; /* Set to non-NULL if manual conversion */

    if ((error = CALL(glGetError)()) != GL_NO_ERROR)
    {
        const char *name;
        name = bugle_api_enum_name(error);
        if (name)
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "%s preceeding %s", name, state->name ? state->name : "root");
        else
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "OpenGL error %#08x preceeding %s",
                             (unsigned int) error, state->name ? state->name : "root");
        bugle_log_printf("glstate", "get", BUGLE_LOG_DEBUG,
                         "pname = %s target = %s binding = %s face = %s level = %d object = %u",
                         bugle_api_enum_name(pname),
                         bugle_api_enum_name(state->target),
                         bugle_api_enum_name(state->binding),
                         bugle_api_enum_name(state->face),
                         (int) state->level,
                         (unsigned int) state->object);
    }

    if (state->info->flags & STATE_MULTIPLEX_ACTIVE_TEXTURE)
    {
        CALL(glGetIntegerv)(GL_ACTIVE_TEXTURE, &old_unit);
        CALL(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &old_client_unit);
        CALL(glActiveTexture)(state->unit);
        if (state->unit > get_texture_coord_units())
            CALL(glClientActiveTexture)(state->unit);
        flag_active_texture = true;
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_BUFFER)
    {
        CALL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &old_buffer);
        CALL(glBindBuffer)(GL_ARRAY_BUFFER, state->object);
    }
#ifdef GL_OES_framebuffer_object
    if ((state->info->flags & STATE_MULTIPLEX_BIND_FRAMEBUFFER)
        && bugle_gl_has_extension_group(BUGLE_GL_OES_framebuffer_object))
    {
        CALL(glGetIntegerv)(state->binding, &old_framebuffer);
        CALL(glBindFramebufferOES)(state->target, state->object);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_RENDERBUFFER)
    {
        CALL(glGetIntegerv)(state->binding, &old_renderbuffer);
        CALL(glBindRenderbufferOES)(state->target, state->object);
    }
#endif
    if ((state->info->flags & STATE_MULTIPLEX_BIND_TEXTURE)
        && state->binding) /* binding of 0 means a proxy texture */
    {
        CALL(glGetIntegerv)(state->binding, &old_texture);
        CALL(glBindTexture)(state->target, state->object);
    }

    switch (state->info->flags & STATE_MODE_MASK)
    {
    case STATE_MODE_GLOBAL:
        if (state->info->type == TYPE_PKc)
        {
            str = (char *) CALL(glGetString)(pname);
            if (str) str = xstrdup(str);
            else str = xstrdup("(nil)");
        }
        else if (state->info->type == TYPE_9GLboolean)
            CALL(glGetBooleanv)(pname, b);
        else if (state->info->type == TYPE_P6GLvoid)
            CALL(glGetPointerv)(pname, p);
        else if (state->info->type == TYPE_7GLfloat)
            CALL(glGetFloatv)(pname, f);
        else
        {
            CALL(glGetIntegerv)(pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_ENABLED:
        b[0] = CALL(glIsEnabled)(pname);
        in_type = TYPE_9GLboolean;
        break;
    case STATE_MODE_TEXTURE_ENV:
    case STATE_MODE_POINT_SPRITE:
        get_helper(state, f, i, &in_type,
                   CALL(glGetTexEnvfv), CALL(glGetTexEnviv));
        break;
    case STATE_MODE_TEX_PARAMETER:
        get_helper(state, f, i, &in_type,
                   CALL(glGetTexParameterfv), CALL(glGetTexParameteriv));
        break;
    case STATE_MODE_LIGHT:
        get_helper(state, f, i, &in_type,
                   CALL(glGetLightfv), NULL);
        break;
    case STATE_MODE_MATERIAL:
        get_helper(state, f, i, &in_type,
                   CALL(glGetMaterialfv), NULL);
        break;
    case STATE_MODE_CLIP_PLANE:
        CALL(glGetClipPlanef)(state->target, f);
        in_type = TYPE_7GLfloat;
        break;
    case STATE_MODE_BUFFER_PARAMETER:
#ifdef GL_OES_map_buffer
        if (state->info->type == TYPE_P6GLvoid)
            CALL(glGetBufferPointervOES)(GL_ARRAY_BUFFER, pname, p);
        else
#endif
        {
            CALL(glGetBufferParameteriv)(GL_ARRAY_BUFFER, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_COMPRESSED_TEXTURE_FORMATS:
        {
            GLint count;
            GLint *formats;
            GLenum *out;

            CALL(glGetIntegerv)(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &count);
            formats = XNMALLOC(count, GLint);
            out = XNMALLOC(count, GLenum);
            CALL(glGetIntegerv)(GL_COMPRESSED_TEXTURE_FORMATS, formats);
            budgie_type_convert(out, TYPE_6GLenum, formats, TYPE_5GLint, count);
            wrapper->data = out;
            wrapper->length = count;
            wrapper->type = TYPE_6GLenum;
            free(formats);
        }
        break;
#ifdef GL_OES_framebuffer_object
    case STATE_MODE_FRAMEBUFFER_ATTACHMENT_PARAMETER:
        CALL(glGetFramebufferAttachmentParameterivOES)(state->target,
                                                       state->level,
                                                       pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_RENDERBUFFER_PARAMETER:
        CALL(glGetRenderbufferParameterivOES)(state->target, pname, i);
        in_type = TYPE_5GLint;
        break;
#endif
    default:
        abort();
    }

    if (flag_active_texture)
    {
        CALL(glActiveTexture)(old_unit);
        if (state->unit > get_texture_coord_units())
            CALL(glClientActiveTexture)(old_client_unit);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_BUFFER)
    {
        CALL(glBindBuffer)(GL_ARRAY_BUFFER, old_buffer);
    }
#ifdef GL_OES_framebuffer_object
    if ((state->info->flags & STATE_MULTIPLEX_BIND_FRAMEBUFFER)
        && bugle_gl_has_extension_group(GL_OES_framebuffer_object))
        CALL(glBindFramebufferOES)(state->target, old_framebuffer);
    if (state->info->flags & STATE_MULTIPLEX_BIND_RENDERBUFFER)
        CALL(glBindRenderbufferOES)(state->target, old_renderbuffer);
#endif
    if ((state->info->flags & STATE_MULTIPLEX_BIND_TEXTURE)
        && state->binding)
        CALL(glBindTexture)(state->target, old_texture);

    if ((error = CALL(glGetError)()) != GL_NO_ERROR)
    {
        const char *name;
        name = bugle_api_enum_name(error);
        if (name)
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "%s generated by state %s",
                             name, state->name ? state->name : "root");
        else
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "OpenGL error %#08x generated by state %s",
                             (unsigned int) error, state->name ? state->name : "root");
        while (CALL(glGetError)() != GL_NO_ERROR)
            ; /* consume any further errors */
        return;
    }

    if (str)
    {
        wrapper->data = str;
        wrapper->length = strlen(str);
        wrapper->type = TYPE_c;
        return;
    }

    if (!wrapper->data)
    {
        switch (in_type)
        {
        case TYPE_7GLfloat: in = f; break;
        case TYPE_5GLint: in = i; break;
        case TYPE_6GLuint: in = ui; break;
        case TYPE_9GLboolean: in = b; break;
        case TYPE_P6GLvoid: in = p; break;
        default: abort();
        }

        wrapper->data = xnmalloc(abs(in_length), budgie_type_size(out_type));
        wrapper->type = out_type;
        wrapper->length = in_length;
        budgie_type_convert(wrapper->data, wrapper->type, in, in_type, abs(wrapper->length));
    }
}

static void dump_wrapper(char **buffer, size_t *size, void *data)
{
    const bugle_state_raw *w;
    int length;

    w = (const bugle_state_raw *) data;
    length = w->length;
    budgie_dump_any_type_extended(w->type, w->data, -1, length, NULL, buffer, size);
}

char *bugle_state_get_string(const glstate *state)
{
    bugle_state_raw wrapper;
    char *ans;

    if (!state->info) return NULL; /* root */
    bugle_state_get_raw(state, &wrapper);
    if (!wrapper.data)
        return "<GL error>";

    if (wrapper.type == TYPE_Pc)
        ans = xstrdup((const char *) wrapper.data); /* bugle_string_io(dump_string_wrapper, (char *) wrapper.data); */
    else
        ans = bugle_string_io(dump_wrapper, &wrapper);
    free(wrapper.data);
    return ans;
}

void bugle_state_get_children(const glstate *self, linked_list *children)
{
    if (self->spawn_children) (*self->spawn_children)(self, children);
    else bugle_list_init(children, free);
}

void bugle_state_clear(glstate *self)
{
    if (self->name) free(self->name);
}

static void spawn_children_buffer_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, buffer_parameter_state, children);
}

#ifdef GL_OES_framebuffer_object
static void spawn_children_framebuffer_attachment(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, framebuffer_attachment_parameter_state, children);
}

static void make_framebuffer_attachment(const glstate *self,
                                        GLenum attachment,
                                        const char *format,
                                        long int index,
                                        linked_list *children)
{
    GLint type;
    glstate *child;

    CALL(glGetFramebufferAttachmentParameterivOES)(self->target, attachment,
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_OES,
                                                  &type);
    if (type != GL_NONE)
    {
        child = XMALLOC(glstate);
        *child = *self;
        if (index < 0) child->name = xstrdup(format);
        else child->name = xasprintf(format, index);
        child->info = NULL;
        child->numeric_name = index;
        child->enum_name = attachment;
        child->level = attachment;
        child->spawn_children = spawn_children_framebuffer_attachment;
        bugle_list_append(children, child);
    }
}

static void spawn_children_framebuffer_object(const glstate *self, linked_list *children)
{
    GLint old;
    GLint attachments;
    int i;

    bugle_list_init(children, free);
    CALL(glGetIntegerv)(self->binding, &old);
    CALL(glBindFramebufferOES)(self->target, self->object);
    make_leaves(self, framebuffer_parameter_state, children);
    if (self->object != 0)
    {
        attachments = 1;
        for (i = 0; i < attachments; i++)
            make_framebuffer_attachment(self, GL_COLOR_ATTACHMENT0_OES + i,
                                        "GL_COLOR_ATTACHMENT%ld",
                                        i, children);
        make_framebuffer_attachment(self, GL_DEPTH_ATTACHMENT_OES,
                                    "GL_DEPTH_ATTACHMENT_OES", -1, children);
        make_framebuffer_attachment(self, GL_STENCIL_ATTACHMENT_OES,
                                    "GL_STENCIL_ATTACHMENT_OES", -1, children);
    }

    CALL(glBindFramebufferOES)(self->target, old);
}

static void spawn_children_framebuffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_objects(self, BUGLE_GLOBJECTS_FRAMEBUFFER, self->target, true,
                 "%lu", spawn_children_framebuffer_object, NULL, children);
}

static void spawn_children_renderbuffer_object(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, renderbuffer_parameter_state, children);
}

static void spawn_children_renderbuffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_objects(self, BUGLE_GLOBJECTS_RENDERBUFFER, self->target, false,
                 "%lu", spawn_children_renderbuffer_object, NULL, children);
}
#endif /* GL_OES_framebuffer_object */

static void spawn_children_global(const glstate *self, linked_list *children)
{
    const char *version;

    version = (const char *) CALL(glGetString)(GL_VERSION);
    bugle_list_init(children, free);
    make_leaves(self, global_state, children);

    if (bugle_gl_has_extension_group(BUGLE_GL_VERSION_ES_CM_1_1))
    {
        make_objects(self, BUGLE_GLOBJECTS_BUFFER, GL_NONE, false,
                     "Buffer[%lu]", spawn_children_buffer_parameter, NULL, children);
    }
#ifdef GL_OES_framebuffer_object
    if (bugle_gl_has_extension_group(BUGLE_GL_OES_framebuffer_object))
    {
        make_target(self, "GL_FRAMEBUFFER_OES",
                    GL_FRAMEBUFFER_OES,
                    GL_FRAMEBUFFER_BINDING_OES,
                    spawn_children_framebuffer, NULL, children);
        make_target(self, "GL_RENDERBUFFER_OES",
                    GL_RENDERBUFFER_OES,
                    GL_RENDERBUFFER_BINDING_OES,
                    spawn_children_renderbuffer, NULL, children);
    }
#endif
}

const glstate *bugle_state_get_root(void)
{
    static const glstate root =
    {
        "", 0, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE,
        0, 0, NULL, spawn_children_global
    };

    return &root;
}
