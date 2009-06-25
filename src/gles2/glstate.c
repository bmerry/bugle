/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2009  Bruce Merry
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
#include <bugle/log.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include <bugle/io.h>
#include <bugle/gl/glsl.h>
#include <bugle/gl/glstate.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/globjects.h>
#include <bugle/gl/glextensions.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include <budgie/call.h>
#include "budgielib/defines.h"
#include "apitables.h"
#include "platform/types.h"

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
#define STATE_MODE_TEX_PARAMETER            0x00000006    /* glGetTexParameter */
#define STATE_MODE_VERTEX_ATTRIB            0x00000011    /* glGetVertexAttrib */
#define STATE_MODE_BUFFER_PARAMETER         0x00000014    /* glGetBufferParameter */
#define STATE_MODE_SHADER                   0x00000015    /* glGetShader */
#define STATE_MODE_PROGRAM                  0x00000016    /* glGetProgram */
#define STATE_MODE_SHADER_INFO_LOG          0x00000017    /* glGetShaderInfoLog */
#define STATE_MODE_PROGRAM_INFO_LOG         0x00000018    /* glGetProgramInfoLog */
#define STATE_MODE_SHADER_SOURCE            0x00000019    /* glGetShaderSource */
#define STATE_MODE_UNIFORM                  0x0000001a    /* glGetActiveUniform, glGetUniform */
#define STATE_MODE_ATTRIB_LOCATION          0x0000001b    /* glGetAttribLocation */
#define STATE_MODE_ATTACHED_SHADERS         0x0000001c    /* glGetAttachedShaders */
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

/* conveniences */
#define STATE_GLOBAL STATE_MODE_GLOBAL
#define STATE_ENABLED STATE_MODE_ENABLED
#define STATE_TEXTURE_ENV (STATE_MODE_TEXTURE_ENV | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_TEX_UNIT (STATE_MODE_GLOBAL | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_TEX_PARAMETER (STATE_MODE_TEX_PARAMETER | STATE_MULTIPLEX_BIND_TEXTURE)
#define STATE_VERTEX_ATTRIB STATE_MODE_VERTEX_ATTRIB
#define STATE_BUFFER_PARAMETER (STATE_MODE_BUFFER_PARAMETER | STATE_MULTIPLEX_BIND_BUFFER)
#define STATE_SHADER STATE_MODE_SHADER
#define STATE_PROGRAM STATE_MODE_PROGRAM
#define STATE_SHADER_INFO_LOG STATE_MODE_SHADER_INFO_LOG
#define STATE_PROGRAM_INFO_LOG STATE_MODE_PROGRAM_INFO_LOG
#define STATE_SHADER_SOURCE STATE_MODE_SHADER_SOURCE
#define STATE_UNIFORM STATE_MODE_UNIFORM
#define STATE_ATTRIB_LOCATION STATE_MODE_ATTRIB_LOCATION
#define STATE_ATTACHED_SHADERS STATE_MODE_ATTACHED_SHADERS
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

static const enum_list cube_map_face_enums[] =
{
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_POSITIVE_X), BUGLE_GL_ES_VERSION_2_0 },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_NEGATIVE_X), BUGLE_GL_ES_VERSION_2_0 },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_POSITIVE_Y), BUGLE_GL_ES_VERSION_2_0 },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y), BUGLE_GL_ES_VERSION_2_0 },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_POSITIVE_Z), BUGLE_GL_ES_VERSION_2_0 },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z), BUGLE_GL_ES_VERSION_2_0 },
    { NULL, GL_NONE, 0 }
};

static const state_info vertex_attrib_state[] =
{
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_ENABLED), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_POINTER), TYPE_Pv, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_CURRENT_VERTEX_ATTRIB), TYPE_7GLfloat, 4, BUGLE_GL_ES_VERSION_2_0, -1, STATE_VERTEX_ATTRIB },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_unit_state[] =
{
    { STATE_NAME(GL_TEXTURE_BINDING_2D), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_TEX_UNIT },
#ifdef GL_OES_texture3D
    { STATE_NAME(GL_TEXTURE_BINDING_3D_OES), TYPE_5GLint, -1, BUGLE_GL_OES_texture3D, -1, STATE_TEX_UNIT },
#endif
    { STATE_NAME(GL_TEXTURE_BINDING_CUBE_MAP), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_TEX_UNIT },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_parameter_state[] =
{
    { STATE_NAME(GL_TEXTURE_MIN_FILTER), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_MAG_FILTER), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_S), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_T), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_TEX_PARAMETER },
#ifdef GL_OES_texture3D
    { STATE_NAME(GL_TEXTURE_WRAP_R_OES), TYPE_6GLenum, -1, BUGLE_GL_OES_texture3D, -1, STATE_TEX_PARAMETER | STATE_SELECT_NO_2D },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info program_state[] =
{
    { STATE_NAME(GL_DELETE_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM },
    { STATE_NAME(GL_INFO_LOG_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM },
    { "InfoLog", GL_NONE, TYPE_Pc, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM_INFO_LOG },
    { STATE_NAME(GL_ATTACHED_SHADERS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_UNIFORMS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM },
    { STATE_NAME(GL_LINK_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM },
    { STATE_NAME(GL_VALIDATE_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_UNIFORM_MAX_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM },
    { "Attached", GL_NONE, TYPE_5GLint, 0, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ATTACHED_SHADERS },
    { STATE_NAME(GL_ACTIVE_ATTRIBUTES), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_PROGRAM },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info shader_state[] =
{
    { STATE_NAME(GL_DELETE_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_SHADER },
    { STATE_NAME(GL_INFO_LOG_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_SHADER },
    { "InfoLog", GL_NONE, TYPE_Pc, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_SHADER_INFO_LOG },
    { "ShaderSource", GL_NONE, TYPE_Pc, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_SHADER_SOURCE },
    { STATE_NAME(GL_SHADER_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_SHADER },
    { STATE_NAME(GL_COMPILE_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_SHADER },
    { STATE_NAME(GL_SHADER_SOURCE_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_SHADER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info generic_enable = { NULL, GL_NONE, TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED };

/* All the make functions _append_ nodes to children. That means you have
 * to initialise the list yourself.
 */
static void make_leaves_conditional(const glstate *self, const state_info *table,
                                    bugle_uint32_t flags, unsigned int mask,
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
                child = BUGLE_MALLOC(glstate);
                *child = *self; /* copies contextual info */
                child->name = bugle_strdup(info->name);
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
        child = BUGLE_MALLOC(glstate);
        *child = *self;
        child->info = info;
        child->name = bugle_asprintf(format, (unsigned long) i);
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

    child = BUGLE_MALLOC(glstate);
    *child = *self;
    child->target = target;
    child->info = info;
    child->name = bugle_asprintf(format, (unsigned long) id);
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
                         bugle_bool add_zero,
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

    child = BUGLE_MALLOC(glstate);
    *child = *self;
    child->name = bugle_strdup(name);
    child->numeric_name = 0;
    child->enum_name = target;
    child->target = target;
    child->face = target;   /* Changed at next level for cube maps */
    child->binding = binding;
    child->spawn_children = spawn_children;
    child->info = info;
    bugle_list_append(children, child);
}

/* Spawning callback functions. These are called either from the make
 * functions (2-argument functions) or from make_leaves conditional due to
 * a state_info with a callback (3-argument form).
 */

static void spawn_children_vertex_attrib(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves_conditional(self, vertex_attrib_state,
                            0,
                            (self->object == 0) ? STATE_SELECT_NON_ZERO : 0,
                            children);
}

static void spawn_vertex_attrib_arrays(const struct glstate *self,
                                       linked_list *children,
                                       const struct state_info *info)
{
    GLint count, i;
    CALL(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &count);
    for (i = 0; i < count; i++)
        make_object(self, 0, "VertexAttrib[%lu]", i, spawn_children_vertex_attrib, NULL, children);
}

static int get_total_texture_units(void)
{
    GLint max = 1;

    CALL(glGetIntegerv)(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max);
    return max;
}

/* Returns a mask of flags not to select from state tables, based on the
 * dimension of the target.
 */
static bugle_uint32_t texture_mask(GLenum target)
{
    bugle_uint32_t mask = 0;

    switch (target)
    {
    case GL_TEXTURE_2D: mask |= STATE_SELECT_NO_2D; break;
    case GL_TEXTURE_CUBE_MAP: mask |= STATE_SELECT_NO_2D; break;
    }
    return mask;
}

static void spawn_children_tex_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves_conditional(self, tex_parameter_state, 0,
                            texture_mask(self->target), children);
}

static void spawn_children_tex_target(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_objects(self, BUGLE_GLOBJECTS_TEXTURE, self->target, BUGLE_TRUE, "%lu",
                 spawn_children_tex_parameter, NULL, children);
}

static void spawn_children_tex_unit(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, tex_unit_state, children);
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
#ifdef GL_OES_texture3D
    if (bugle_gl_has_extension_group(BUGLE_GL_OES_texture3D))
    {
        make_target(self, "GL_TEXTURE_3D_OES", GL_TEXTURE_3D_OES,
                        GL_TEXTURE_BINDING_3D_OES, spawn_children_tex_target, NULL, children);
    }
#endif
    make_target(self, "GL_TEXTURE_CUBE_MAP",
                GL_TEXTURE_CUBE_MAP,
                GL_TEXTURE_BINDING_CUBE_MAP,
                spawn_children_tex_target, NULL, children);
}

static void spawn_children_shader(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, shader_state, children);
}

static void spawn_children_program(const glstate *self, linked_list *children)
{
    GLint i, count, max, status;
    GLenum type;
    GLsizei length, size;
    glstate *child;
    static const state_info uniform_state =
    {
        NULL, GL_NONE, NULL_TYPE, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_UNIFORM
    };
    static const state_info attrib_state =
    {
        NULL, GL_NONE, TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ATTRIB_LOCATION
    };

    bugle_list_init(children, bugle_free);
    make_leaves(self, program_state, children);

    bugle_glGetProgramiv(self->object, GL_ACTIVE_UNIFORMS, &count);
    bugle_glGetProgramiv(self->object, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max);
    /* If the program failed to link, then uniforms cannot be queried */
    bugle_glGetProgramiv(self->object, GL_LINK_STATUS, &status);
    if (status != GL_FALSE)
        for (i = 0; i < count; i++)
        {
            child = BUGLE_MALLOC(glstate);
            *child = *self;
            child->spawn_children = NULL;
            child->info = &uniform_state;
            child->name = BUGLE_NMALLOC(max, char);
            child->name[0] = '\0'; /* sanity, in case the query borks */
            child->enum_name = 0;
            child->level = i;      /* level is overloaded with the uniform index */
            bugle_glGetActiveUniform(self->object, i, max, &length, &size,
                                     &type, child->name);
            if (length)
            {
                child->numeric_name = bugle_glGetUniformLocation(self->object, child->name);
                /* Check for built-in state, which is returned by glGetActiveUniformARB
                 * but cannot be queried.
                 */
                if (child->numeric_name == -1) length = 0;
            }
            if (length) bugle_list_append(children, child);
            else bugle_free(child->name); /* failed query */
        }

    if (status != GL_FALSE)
    {
        bugle_glGetProgramiv(self->object, GL_ACTIVE_ATTRIBUTES,
                             &count);
        bugle_glGetProgramiv(self->object, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                             &max);

        for (i = 0; i < count; i++)
        {
            child = BUGLE_MALLOC(glstate);
            *child = *self;
            child->spawn_children = NULL;
            child->info = &attrib_state;
            child->name = BUGLE_NMALLOC(max + 1, char);
            child->name[0] = '\0';
            child->numeric_name = i;
            child->enum_name = 0;
            child->level = i;
            bugle_glGetActiveAttrib(self->object, i, max, &length, &size,
                                    &type, child->name);
            if (length) bugle_list_append(children, child);
            else bugle_free(child->name);
        }
    }
}

static void spawn_shaders(const struct glstate *self,
                          linked_list *children,
                          const struct state_info *info)
{
    make_objects(self, BUGLE_GLOBJECTS_SHADER, GL_NONE, BUGLE_FALSE,
                 "Shader[%lu]", spawn_children_shader, NULL, children);
}

static void spawn_programs(const struct glstate *self,
                           linked_list *children,
                           const struct state_info *info)
{
    make_objects(self, BUGLE_GLOBJECTS_PROGRAM, GL_NONE, BUGLE_FALSE,
                 "Program[%lu]", spawn_children_program, NULL, children);
}

/*** Main state table ***/

static const state_info global_state[] =
{
    { "Attribute Arrays", GL_NONE, NULL_TYPE, -1, BUGLE_GL_ES_VERSION_2_0, -1, 0, spawn_vertex_attrib_arrays },
    { STATE_NAME(GL_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ELEMENT_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VIEWPORT), TYPE_5GLint, 4, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_RANGE), TYPE_7GLfloat, 2, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },

    { STATE_NAME(GL_LINE_WIDTH), TYPE_7GLfloat, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CULL_FACE), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_CULL_FACE_MODE), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRONT_FACE), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_FACTOR), TYPE_7GLfloat, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_UNITS), TYPE_7GLfloat, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_FILL), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_ALPHA_TO_COVERAGE), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_COVERAGE), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_COVERAGE_VALUE), TYPE_7GLfloat, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLE_COVERAGE_INVERT), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { "Texture units", GL_TEXTURE0, NULL_TYPE, -1, BUGLE_GL_ES_VERSION_2_0, -1, 0, spawn_texture_units },
    { "Textures", GL_TEXTURE_2D, NULL_TYPE, -1, BUGLE_GL_ES_VERSION_2_0, -1, 0, spawn_textures },
    { STATE_NAME(GL_ACTIVE_TEXTURE), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SCISSOR_TEST), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_SCISSOR_BOX), TYPE_5GLint, 4, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_TEST), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_STENCIL_FUNC), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_VALUE_MASK), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_REF), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_FAIL), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_PASS_DEPTH_FAIL), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_PASS_DEPTH_PASS), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_FUNC), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_VALUE_MASK), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_REF), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_FAIL), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_PASS_DEPTH_FAIL), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_PASS_DEPTH_PASS), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_TEST), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_DEPTH_FUNC), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_BLEND_SRC_RGB), TYPE_11GLblendenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_SRC_ALPHA), TYPE_11GLblendenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_DST_RGB), TYPE_11GLblendenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_DST_ALPHA), TYPE_11GLblendenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_EQUATION_RGB), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_EQUATION_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_COLOR), TYPE_7GLfloat, 4, BUGLE_EXTGROUP_blend_color, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DITHER), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_WRITEMASK), TYPE_9GLboolean, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_WRITEMASK), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_WRITEMASK), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_CLEAR_VALUE), TYPE_7GLfloat, 4, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_CLEAR_VALUE), TYPE_7GLfloat, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_CLEAR_VALUE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },

    { "Shader objects", 0, NULL_TYPE, -1, BUGLE_GL_ES_VERSION_2_0, -1, 0, spawn_shaders },
    { STATE_NAME(GL_CURRENT_PROGRAM), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { "Program objects", 0, NULL_TYPE, -1, BUGLE_GL_ES_VERSION_2_0, -1, 0, spawn_programs },
    { STATE_NAME(GL_GENERATE_MIPMAP_HINT), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
#if defined(GL_OES_standard_derivatives) && defined(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES)
    { STATE_NAME_EXT(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, _OES), TYPE_6GLenum, -1, BUGLE_GL_OES_standard_derivatives, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_SUBPIXEL_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
#ifdef GL_OES_texture3D
    { STATE_NAME(GL_MAX_3D_TEXTURE_SIZE_OES), TYPE_5GLint, -1, BUGLE_GL_OES_texture3D, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_MAX_TEXTURE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CUBE_MAP_TEXTURE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VIEWPORT_DIMS), TYPE_5GLint, 2, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_POINT_SIZE_RANGE), TYPE_7GLfloat, 2, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_LINE_WIDTH_RANGE), TYPE_7GLfloat, 2, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLE_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COMPRESSED_TEXTURE_FORMATS), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_COMPRESSED_TEXTURE_FORMATS },
    { STATE_NAME(GL_NUM_COMPRESSED_TEXTURE_FORMATS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EXTENSIONS), TYPE_PKc, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RENDERER), TYPE_PKc, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SHADING_LANGUAGE_VERSION), TYPE_PKc, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VENDOR), TYPE_PKc, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERSION), TYPE_PKc, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_ATTRIBS), TYPE_5GLint, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_UNIFORM_VECTORS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VARYING_VECTORS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_EXTGROUP_texunits, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_UNIFORM_VECTORS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    /* FIXME: glGetError */

#ifdef GL_EXT_texture_filter_anisotropic
    { STATE_NAME(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT), TYPE_7GLfloat, -1, BUGLE_GL_EXT_texture_filter_anisotropic, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_RENDERBUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRAMEBUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_RENDERBUFFER_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_GLOBAL },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info buffer_parameter_state[] =
{
    { STATE_NAME(GL_BUFFER_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_USAGE), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_BUFFER_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info renderbuffer_parameter_state[] =
{
    { STATE_NAME(GL_RENDERBUFFER_WIDTH), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_HEIGHT), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_INTERNAL_FORMAT), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_RED_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_GREEN_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_BLUE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_ALPHA_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_DEPTH_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_STENCIL_SIZE), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_RENDERBUFFER_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info framebuffer_parameter_state[] =
{
    { STATE_NAME(GL_SAMPLE_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ALPHA_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_DEPTH_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_STENCIL_BITS), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_STENCIL_REF), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info framebuffer_attachment_parameter_state[] =
{
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE), TYPE_6GLenum, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL), TYPE_5GLint, -1, BUGLE_GL_ES_VERSION_2_0, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
#ifdef GL_OES_texture3D
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_OES), TYPE_5GLint, -1, BUGLE_GL_OES_texture3D, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
#endif
   { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

/* This exists to simplify the work of gldump.c */
const state_info * const all_state[] =
{
    global_state,
    tex_parameter_state,
    tex_unit_state,
    vertex_attrib_state,
    buffer_parameter_state,
    shader_state,
    program_state,
    framebuffer_attachment_parameter_state,
    framebuffer_parameter_state,
    renderbuffer_parameter_state,
    NULL
};

static void get_helper(const glstate *state,
                       GLfloat *f, GLint *i,
                       budgie_type *in_type,
                       void (BUDGIEAPIP get_float)(GLenum, GLenum, GLfloat *),
                       void (BUDGIEAPIP get_int)(GLenum, GLenum, GLint *))
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

static void uniform_types(GLenum type,
                          budgie_type *in_type,
                          budgie_type *out_type)
{
    switch (type)
    {
    case GL_FLOAT: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_VEC2: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_VEC3: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_VEC4: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_INT: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
    case GL_INT_VEC2: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
    case GL_INT_VEC3: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
    case GL_INT_VEC4: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
    case GL_BOOL: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; break;
    case GL_BOOL_VEC2: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; break;
    case GL_BOOL_VEC3: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; break;
    case GL_BOOL_VEC4: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; break;
    case GL_FLOAT_MAT2: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_MAT3: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_MAT4: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
#if GL_OES_texture3D
    case GL_SAMPLER_3D_OES:
#endif
        *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
    default:
        abort();
    }
}
void bugle_state_get_raw(const glstate *state, bugle_state_raw *wrapper)
{
    GLenum error;
    GLfloat f[16];
    GLint i[16];
    GLuint ui[16];
    GLboolean b[16];
    void *p[16];
    GLsizei length;
    GLint max_length;

    void *in;
    budgie_type in_type, out_type;
    int in_length;
    char *str = NULL;  /* Set to non-NULL for human-readable string output */

    GLint old_texture, old_buffer;
    GLint old_unit, old_framebuffer, old_renderbuffer;
    bugle_bool flag_active_texture = BUGLE_FALSE;
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
        name = bugle_api_enum_name(error, BUGLE_API_EXTENSION_BLOCK_GL);
        if (name)
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "%s preceeding %s", name, state->name ? state->name : "root");
        else
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "OpenGL error %#08x preceeding %s",
                             (unsigned int) error, state->name ? state->name : "root");
        bugle_log_printf("glstate", "get", BUGLE_LOG_DEBUG,
                         "pname = %s target = %s binding = %s face = %s level = %d object = %u",
                         bugle_api_enum_name(pname, BUGLE_API_EXTENSION_BLOCK_GL),
                         bugle_api_enum_name(state->target, BUGLE_API_EXTENSION_BLOCK_GL),
                         bugle_api_enum_name(state->binding, BUGLE_API_EXTENSION_BLOCK_GL),
                         bugle_api_enum_name(state->face, BUGLE_API_EXTENSION_BLOCK_GL),
                         (int) state->level,
                         (unsigned int) state->object);
    }

    if (state->info->flags & STATE_MULTIPLEX_ACTIVE_TEXTURE)
    {
        CALL(glGetIntegerv)(GL_ACTIVE_TEXTURE, &old_unit);
        CALL(glActiveTexture)(state->unit);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_BUFFER)
    {
        CALL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &old_buffer);
        CALL(glBindBuffer)(GL_ARRAY_BUFFER, state->object);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_FRAMEBUFFER)
    {
        CALL(glGetIntegerv)(state->binding, &old_framebuffer);
        CALL(glBindFramebuffer)(state->target, state->object);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_RENDERBUFFER)
    {
        CALL(glGetIntegerv)(state->binding, &old_renderbuffer);
        CALL(glBindRenderbuffer)(state->target, state->object);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_TEXTURE)
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
            if (str) str = bugle_strdup(str);
            else str = bugle_strdup("(nil)");
        }
        else if (state->info->type == TYPE_9GLboolean)
            CALL(glGetBooleanv)(pname, b);
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
    case STATE_MODE_TEX_PARAMETER:
        get_helper(state, f, i, &in_type,
                   CALL(glGetTexParameterfv), CALL(glGetTexParameteriv));
        break;
    case STATE_MODE_VERTEX_ATTRIB:
        if (state->info->type == TYPE_Pv)
            CALL(glGetVertexAttribPointerv)(state->object, pname, p);
        else if (state->info->type == TYPE_7GLfloat)
            CALL(glGetVertexAttribfv)(state->object, pname, f);
        else
        {
            CALL(glGetVertexAttribiv)(state->object, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_BUFFER_PARAMETER:
        CALL(glGetBufferParameteriv)(GL_ARRAY_BUFFER, pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_SHADER:
        bugle_glGetShaderiv(state->object, pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_PROGRAM:
        bugle_glGetProgramiv(state->object, pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_SHADER_INFO_LOG:
        max_length = 1;
        bugle_glGetShaderiv(state->object, GL_INFO_LOG_LENGTH, &max_length);
        if (max_length < 1) max_length = 1;
        str = BUGLE_NMALLOC(max_length, char);
        bugle_glGetShaderInfoLog(state->object, max_length, &length, str);
        /* AMD don't always nul-terminate empty strings */
        if (length >= 0 && length < max_length) str[length] = '\0';
        break;
    case STATE_MODE_PROGRAM_INFO_LOG:
        max_length = 1;
        bugle_glGetProgramiv(state->object, GL_INFO_LOG_LENGTH, &max_length);
        if (max_length < 1) max_length = 1;
        str = BUGLE_NMALLOC(max_length, char);
        bugle_glGetProgramInfoLog(state->object, max_length, &length, str);
        /* AMD don't always nul-terminate empty strings */
        if (length >= 0 && length < max_length) str[length] = '\0';
        break;
    case STATE_MODE_SHADER_SOURCE:
        max_length = 1;
        bugle_glGetShaderiv(state->object, GL_SHADER_SOURCE_LENGTH, &max_length);
        if (max_length < 1) max_length = 1;
        str = BUGLE_NMALLOC(max_length, char);
        bugle_glGetShaderSource(state->object, max_length, &length, str);
        /* AMD don't always nul-terminate empty strings */
        if (length >= 0 && length < max_length) str[length] = '\0';
        break;
    case STATE_MODE_UNIFORM:
        {
            GLsizei size;
            GLenum type;
            char dummy[1];
            GLsizei units;  /* number of the base type in the full type */
            budgie_type composite_type;
            void *buffer;

            bugle_glGetActiveUniform(state->object, state->level, 1, NULL,
                                     &size, &type, dummy);
            uniform_types(type, &in_type, &out_type);
            composite_type = bugle_gl_type_to_type(type);
            units = budgie_type_size(composite_type) / budgie_type_size(in_type);
            buffer = bugle_malloc(size * units * budgie_type_size(in_type));
            wrapper->data = bugle_malloc(size * units * budgie_type_size(out_type));
            wrapper->type = composite_type;
            if (size == 1)
                wrapper->length = -1;
            else
                wrapper->length = size;
            if (in_type == TYPE_7GLfloat)
                bugle_glGetUniformfv(state->object, state->numeric_name, (GLfloat *) buffer);
            else
                bugle_glGetUniformiv(state->object, state->numeric_name, (GLint *) buffer);

            budgie_type_convert(wrapper->data, out_type, buffer, in_type, size * units);
            bugle_free(buffer);
        }
        break;
    case STATE_MODE_ATTRIB_LOCATION:
        i[0] = bugle_glGetAttribLocation(state->object, state->name);
        break;
    case STATE_MODE_ATTACHED_SHADERS:
        {
            GLuint *attached;

            max_length = 1;
            bugle_glGetProgramiv(state->object, GL_ATTACHED_SHADERS, &max_length);
            attached = BUGLE_NMALLOC(max_length, GLuint);
            bugle_glGetAttachedShaders(state->object, max_length, NULL, attached);
            wrapper->data = attached;
            wrapper->length = max_length;
            wrapper->type = TYPE_6GLuint;
        }
        break;
    case STATE_MODE_COMPRESSED_TEXTURE_FORMATS:
        {
            GLint count;
            GLint *formats;
            GLenum *out;

            CALL(glGetIntegerv)(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &count);
            formats = BUGLE_NMALLOC(count, GLint);
            out = BUGLE_NMALLOC(count, GLenum);
            CALL(glGetIntegerv)(GL_COMPRESSED_TEXTURE_FORMATS, formats);
            budgie_type_convert(out, TYPE_6GLenum, formats, TYPE_5GLint, count);
            wrapper->data = out;
            wrapper->length = count;
            wrapper->type = TYPE_6GLenum;
            bugle_free(formats);
        }
        break;
    case STATE_MODE_FRAMEBUFFER_ATTACHMENT_PARAMETER:
        CALL(glGetFramebufferAttachmentParameteriv)(state->target,
                                                    state->level,
                                                    pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_RENDERBUFFER_PARAMETER:
        CALL(glGetRenderbufferParameteriv)(state->target, pname, i);
        in_type = TYPE_5GLint;
        break;
    default:
        abort();
    }

    if (flag_active_texture)
        CALL(glActiveTexture)(old_unit);
    if (state->info->flags & STATE_MULTIPLEX_BIND_BUFFER)
        CALL(glBindBuffer)(GL_ARRAY_BUFFER, old_buffer);
    if (state->info->flags & STATE_MULTIPLEX_BIND_FRAMEBUFFER)
        CALL(glBindFramebuffer)(state->target, old_framebuffer);
    if (state->info->flags & STATE_MULTIPLEX_BIND_RENDERBUFFER)
        CALL(glBindRenderbuffer)(state->target, old_renderbuffer);
    if (state->info->flags & STATE_MULTIPLEX_BIND_TEXTURE)
        CALL(glBindTexture)(state->target, old_texture);

    if ((error = CALL(glGetError)()) != GL_NO_ERROR)
    {
        const char *name;
        name = bugle_api_enum_name(error, BUGLE_API_EXTENSION_BLOCK_GL);
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
        case TYPE_Pv: in = p; break;
        default: abort();
        }

        wrapper->data = bugle_nmalloc(abs(in_length), budgie_type_size(out_type));
        wrapper->type = out_type;
        wrapper->length = in_length;
        budgie_type_convert(wrapper->data, wrapper->type, in, in_type, abs(wrapper->length));
    }
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
        ans = bugle_strdup((const char *) wrapper.data);
    else
    {
        bugle_io_writer *writer;

        writer = bugle_io_writer_mem_new(64);
        budgie_dump_any_type_extended(wrapper.type, wrapper.data, -1, wrapper.length, NULL, writer);
        ans = bugle_io_writer_mem_get(writer);
        bugle_io_writer_close(writer);
    }
    bugle_free(wrapper.data);
    return ans;
}

void bugle_state_get_children(const glstate *self, linked_list *children)
{
    if (self->spawn_children) (*self->spawn_children)(self, children);
    else bugle_list_init(children, bugle_free);
}

void bugle_state_clear(glstate *self)
{
    if (self->name) bugle_free(self->name);
}

static void spawn_children_buffer_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, buffer_parameter_state, children);
}


static void spawn_children_framebuffer_attachment(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
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

    CALL(glGetFramebufferAttachmentParameteriv)(self->target, attachment,
                                                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                &type);
    if (type != GL_NONE)
    {
        child = BUGLE_MALLOC(glstate);
        *child = *self;
        if (index < 0) child->name = bugle_strdup(format);
        else child->name = bugle_asprintf(format, index);
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

    bugle_list_init(children, bugle_free);
    CALL(glGetIntegerv)(self->binding, &old);
    CALL(glBindFramebuffer)(self->target, self->object);
    make_leaves(self, framebuffer_parameter_state, children);
    if (self->object != 0)
    {
        attachments = 1;
        for (i = 0; i < attachments; i++)
            make_framebuffer_attachment(self, GL_COLOR_ATTACHMENT0 + i,
                                        "GL_COLOR_ATTACHMENT%ld",
                                        i, children);
        make_framebuffer_attachment(self, GL_DEPTH_ATTACHMENT,
                                    "GL_DEPTH_ATTACHMENT", -1, children);
        make_framebuffer_attachment(self, GL_STENCIL_ATTACHMENT,
                                    "GL_STENCIL_ATTACHMENT", -1, children);
    }

    CALL(glBindFramebuffer)(self->target, old);
}

static void spawn_children_framebuffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_objects(self, BUGLE_GLOBJECTS_FRAMEBUFFER, self->target, BUGLE_TRUE,
                 "%lu", spawn_children_framebuffer_object, NULL, children);
}

static void spawn_children_renderbuffer_object(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, renderbuffer_parameter_state, children);
}

static void spawn_children_renderbuffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_objects(self, BUGLE_GLOBJECTS_RENDERBUFFER, self->target, BUGLE_FALSE,
                 "%lu", spawn_children_renderbuffer_object, NULL, children);
}

static void spawn_children_global(const glstate *self, linked_list *children)
{
    const char *version;

    version = (const char *) CALL(glGetString)(GL_VERSION);
    bugle_list_init(children, bugle_free);
    make_leaves(self, global_state, children);

    make_objects(self, BUGLE_GLOBJECTS_BUFFER, GL_NONE, BUGLE_FALSE,
                 "Buffer[%lu]", spawn_children_buffer_parameter, NULL, children);
    make_target(self, "GL_FRAMEBUFFER",
                GL_FRAMEBUFFER,
                GL_FRAMEBUFFER_BINDING,
                spawn_children_framebuffer, NULL, children);
    make_target(self, "GL_RENDERBUFFER",
                GL_RENDERBUFFER,
                GL_RENDERBUFFER_BINDING,
                spawn_children_renderbuffer, NULL, children);
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
