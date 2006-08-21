/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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
#define _XOPEN_SOURCE 500
#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/tracker.h"
#include "src/glstate.h"
#include "src/glexts.h"
#include "src/glsl.h"
#include "common/hashtable.h"
#include "common/protocol.h"
#include "common/bool.h"
#include "common/safemem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

static int in_pipe = -1, out_pipe = -1;
static bool break_on[NUMBER_OF_FUNCTIONS];
static bool break_on_error = true, break_on_next = false;
static bool stopped = true;
static uint32_t start_id = 0;

static void send_state(const glstate *state, uint32_t id)
{
    char *str;
    bugle_linked_list children;
    bugle_list_node *cur;

    str = bugle_state_get_string(state);
    gldb_protocol_send_code(out_pipe, RESP_STATE_NODE_BEGIN);
    gldb_protocol_send_code(out_pipe, id);
    if (state->name) gldb_protocol_send_string(out_pipe, state->name);
    else gldb_protocol_send_string(out_pipe, "");
    gldb_protocol_send_code(out_pipe, state->numeric_name);
    gldb_protocol_send_code(out_pipe, state->enum_name);
    if (str) gldb_protocol_send_string(out_pipe, str);
    else gldb_protocol_send_string(out_pipe, "");
    free(str);

    bugle_state_get_children(state, &children);
    for (cur = bugle_list_head(&children); cur; cur = bugle_list_next(cur))
    {
        send_state((const glstate *) bugle_list_data(cur), id);
        bugle_state_clear((glstate *) bugle_list_data(cur));
    }
    bugle_list_clear(&children);

    gldb_protocol_send_code(out_pipe, RESP_STATE_NODE_END);
    gldb_protocol_send_code(out_pipe, id);
}

static void dump_ppm_header(FILE *f, void *data)
{
    int *wh;

    wh = (int *) data;
    fprintf(f, "P6\n%d %d\n255\n", wh[0], wh[1]);
}

static bool debugger_screenshot(int pipe)
{
    Display *dpy;
    GLXContext aux, real;
    GLXDrawable old_read, old_write;
    unsigned int width, height, stride;
    int wh[2];
    size_t header_len;
    char *header;
    char *data, *in, *out;
    unsigned int i;

    aux = bugle_get_aux_context();
    if (!aux || !bugle_begin_internal_render()) return false;
    real = CALL_glXGetCurrentContext();
    old_write = CALL_glXGetCurrentDrawable();
    old_read = CALL_glXGetCurrentReadDrawable();
    dpy = CALL_glXGetCurrentDisplay();
    CALL_glXQueryDrawable(dpy, old_write, GLX_WIDTH, &width);
    CALL_glXQueryDrawable(dpy, old_write, GLX_HEIGHT, &height);
    CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);

    wh[0] = width;
    wh[1] = height;
    stride = ((3 * width + 3) & ~3);
    header = budgie_string_io(dump_ppm_header, wh);
    data = bugle_malloc(stride * height);
    CALL_glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);

    header_len = strlen(header);
    header = bugle_realloc(header, header_len + width * height * 3);
    in = data + (height - 1) * stride;
    out = header + header_len;
    for (i = 0; i < height; i++)
    {
        memcpy(out, in, width * 3);
        out += width * 3;
        in -= stride;
    }

    gldb_protocol_send_code(pipe, RESP_SCREENSHOT);
    gldb_protocol_send_binary_string(pipe, header_len + width * height * 3, header);
    free(header);
    free(data);

    CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
    bugle_end_internal_render("debugger_screenshot", true);
    return true;
}

static budgie_function find_function(const char *name)
{
    int i;

    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        if (strcmp(budgie_function_table[i].name, name) == 0)
            return i;
    return NULL_FUNCTION;
}

static void dump_any_call_string_io(FILE *out, void *data)
{
    budgie_dump_any_call(&((const function_call *) data)->generic, 0, out);
}

/* Utility functions and structures for image grabbers. When doing
 * grabbing from the primary context, we need to save the current pixel
 * client state so that it can be set to known values, then restored.
 * This is done in a pixel_state struct.
 */
typedef struct
{
    GLboolean swap_bytes, lsb_first;
    GLint row_length, skip_rows;
    GLint skip_pixels, alignment;
#ifdef GL_EXT_texture3D
    GLint image_height, skip_images;
#endif
#ifdef GL_EXT_pixel_buffer_object
    GLint pbo;
#endif
} pixel_state;

/* Save all the old pack state to old and set default state with
 * alignment of 1.
 */
static void pixel_pack_reset(pixel_state *old)
{
    CALL_glGetBooleanv(GL_PACK_SWAP_BYTES, &old->swap_bytes);
    CALL_glGetBooleanv(GL_PACK_LSB_FIRST, &old->lsb_first);
    CALL_glGetIntegerv(GL_PACK_ROW_LENGTH, &old->row_length);
    CALL_glGetIntegerv(GL_PACK_SKIP_ROWS, &old->skip_rows);
    CALL_glGetIntegerv(GL_PACK_SKIP_PIXELS, &old->skip_pixels);
    CALL_glGetIntegerv(GL_PACK_ALIGNMENT, &old->alignment);
    CALL_glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
    CALL_glPixelStorei(GL_PACK_LSB_FIRST, GL_FALSE);
    CALL_glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    CALL_glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    CALL_glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    CALL_glPixelStorei(GL_PACK_ALIGNMENT, 1);
#ifdef GL_EXT_texture3D
    if (bugle_gl_has_extension_group(BUGLE_GL_EXT_texture3D))
    {
        CALL_glGetIntegerv(GL_PACK_IMAGE_HEIGHT, &old->image_height);
        CALL_glGetIntegerv(GL_PACK_SKIP_IMAGES, &old->skip_images);
        CALL_glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
        CALL_glPixelStorei(GL_PACK_SKIP_IMAGES, 0);
    }
#endif
#ifdef GL_EXT_pixel_buffer_object
    if (bugle_gl_has_extension_group(BUGLE_GL_EXT_pixel_buffer_object))
    {
        CALL_glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING_EXT, &old->pbo);
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, 0);
    }
#endif
}

/* Sets the pixel pack state specified in old */
static void pixel_pack_restore(const pixel_state *old)
{
    CALL_glPixelStorei(GL_PACK_SWAP_BYTES, old->swap_bytes);
    CALL_glPixelStorei(GL_PACK_LSB_FIRST, old->lsb_first);
    CALL_glPixelStorei(GL_PACK_ROW_LENGTH, old->row_length);
    CALL_glPixelStorei(GL_PACK_SKIP_ROWS, old->skip_rows);
    CALL_glPixelStorei(GL_PACK_SKIP_PIXELS, old->skip_pixels);
    CALL_glPixelStorei(GL_PACK_ALIGNMENT, old->alignment);
#ifdef GL_EXT_texture3D
    if (bugle_gl_has_extension_group(BUGLE_GL_EXT_texture3D))
    {
        CALL_glPixelStorei(GL_PACK_IMAGE_HEIGHT, old->image_height);
        CALL_glPixelStorei(GL_PACK_SKIP_IMAGES, old->skip_images);
    }
#endif
#ifdef GL_EXT_pixel_buffer_object
    if (bugle_gl_has_extension_group(BUGLE_GL_EXT_pixel_buffer_object))
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, old->pbo);
#endif
}

/* Wherever possible we use the aux context. However, default textures
 * are not shared between contexts, so we sometimes have to take our
 * chances with setting up the default readback state and restoring it
 * afterwards.
 */
static bool send_data_texture(uint32_t id, GLuint texid, GLenum target,
                              GLenum face, GLint level,
                              GLenum format, GLenum type)
{
    char *data;
    size_t length;
    GLint width = 1, height = 1, depth = 1;

    Display *dpy = NULL;
    GLXContext aux = NULL, real = NULL;
    GLXDrawable old_read = 0, old_write = 0;
    pixel_state old_pack;

    if (!bugle_begin_internal_render())
    {
        gldb_protocol_send_code(out_pipe, RESP_ERROR);
        gldb_protocol_send_code(out_pipe, id);
        gldb_protocol_send_code(out_pipe, 0);
        gldb_protocol_send_string(out_pipe, "inside glBegin/glEnd");
        return false;
    }

    aux = bugle_get_aux_context();
    if (aux && texid)
    {
        real = CALL_glXGetCurrentContext();
        old_write = CALL_glXGetCurrentDrawable();
        old_read = CALL_glXGetCurrentReadDrawable();
        dpy = CALL_glXGetCurrentDisplay();
        CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);

        CALL_glPushAttrib(GL_ALL_ATTRIB_BITS);
        CALL_glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        CALL_glPixelStorei(GL_PACK_ALIGNMENT, 1);
    }
    else
        pixel_pack_reset(&old_pack);

    CALL_glBindTexture(target, texid);

    CALL_glGetTexLevelParameteriv(face, level, GL_TEXTURE_WIDTH, &width);
    if (face != GL_TEXTURE_1D)
        CALL_glGetTexLevelParameteriv(face, level, GL_TEXTURE_HEIGHT, &height);
#ifdef GL_EXT_texture3D
    if (face == GL_TEXTURE_3D_EXT && bugle_gl_has_extension_group(BUGLE_GL_EXT_texture3D))
        CALL_glGetTexLevelParameteriv(face, level, GL_TEXTURE_DEPTH_EXT, &depth);
#endif
    length = bugle_gl_type_to_size(type) * bugle_gl_format_to_count(format, type)
        * width * height * depth;
    data = bugle_malloc(length);

    CALL_glGetTexImage(face, level, format, type, data);

    if (aux && texid)
    {
        GLenum error;
        while ((error = CALL_glGetError()) != GL_NO_ERROR)
            fprintf(stderr, "Warning: GL error %#04x generated by send_data_texture in aux context\n", error);
        CALL_glPopClientAttrib();
        CALL_glPopAttrib();
        CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
    }
    else
        pixel_pack_restore(&old_pack);

    gldb_protocol_send_code(out_pipe, RESP_DATA);
    gldb_protocol_send_code(out_pipe, id);
    gldb_protocol_send_code(out_pipe, REQ_DATA_TEXTURE);
    gldb_protocol_send_binary_string(out_pipe, length, data);
    gldb_protocol_send_code(out_pipe, width);
    gldb_protocol_send_code(out_pipe, height);
    gldb_protocol_send_code(out_pipe, depth);
    bugle_end_internal_render("send_data_texture", true);
    free(data);
    return true;
}

/* Unfortunately, FBO cannot have a size query, because while incomplete
 * it could have different sizes for different buffers. Thus, we have to
 * do the query on the underlying attachment. The return is false if there
 * is no attachment or a size could not be established.
 *
 * It is assumed that the appropriate framebuffer (if any) is already current.
 */
static bool get_framebuffer_size(GLuint fbo, GLenum target, GLenum attachment,
                                 GLint *width, GLint *height)
{
#ifdef GL_EXT_framebuffer_object
    if (fbo)
    {
        GLint type, name, old_name;
        GLenum texture_target, texture_binding;
        GLint level = 0, face;

        CALL_glGetFramebufferAttachmentParameterivEXT(target, attachment,
                                                      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT, &type);
        CALL_glGetFramebufferAttachmentParameterivEXT(target, attachment,
                                                      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, &name);
        if (type == GL_RENDERBUFFER_EXT)
        {
            CALL_glGetInteger(GL_RENDERBUFFER_BINDING_EXT, &old_name);
            CALL_glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, name);
            glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_WIDTH_EXT, width);
            glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_WIDTH_EXT, height);
            CALL_glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, old_name);
        }
        else if (type == GL_TEXTURE)
        {
            CALL_glGetFramebufferAttachmentParameterivEXT(target, attachment,
                                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT, &level);
            texture_target = bugle_trackobjects_get_target(BUGLE_TRACKOBJECTS_TEXTURE, name);
            switch (texture_target)
            {
            case GL_TEXTURE_1D: texture_binding = GL_TEXTURE_BINDING_1D; break;
            case GL_TEXTURE_2D: texture_binding = GL_TEXTURE_BINDING_2D; break;
#ifdef GL_EXT_texture3D
            case GL_TEXTURE_3D: texture_binding = GL_TEXTURE_BINDING_3D; break;
#endif
#ifdef GL_EXT_texture_cube_map
            case GL_TEXTURE_CUBE_MAP_EXT: texture_binding = GL_TEXTURE_BINDING_CUBE_MAP_EXT; break;
#endif
#ifdef GL_NV_texture_rectangle
            case GL_TEXTURE_RECTANGLE_NV: texture_binding = GL_TEXTURE_BINDING_RECTANGLE_NV; break;
#endif
            default:
                return false;
            }

            CALL_glGetIntegerv(texture_binding, &old_name);
            CALL_glBindTexture(texture_target, name);
#ifdef GL_EXT_texture_cube_map
            if (target == GL_TEXTURE_CUBE_MAP_EXT)
            {
                CALL_glGetFramebufferAttachmentParameteriv(target, attachment,
                                                           GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT, &face);
            }
            else
#endif
            {
                face = texture_target;
            }
            glGetTexLevelParameteriv(face, level, GL_TEXTURE_WIDTH, width);
            glGetTexLevelParameteriv(face, level, GL_TEXTURE_HEIGHT, height);
        }
        else
            return false;
    }
    else
#endif
    {
        Display *dpy;
        GLXDrawable draw;
        unsigned int value;

        dpy = CALL_glXGetCurrentDisplay();
        draw = CALL_glXGetCurrentReadDrawable();
        /* Window-system framebuffer */
        CALL_glXQueryDrawable(dpy, draw, GLX_WIDTH, &value);
        *width = value;
        CALL_glXQueryDrawable(dpy, draw, GLX_HEIGHT, &value);
        *height = value;
    }
    return true;
}

/* This function is complicated by GL_EXT_framebuffer_object and
 * GL_EXT_framebuffer_blit. The latter defines separate read and draw
 * framebuffers, and also modifies the semantics of the former (even if
 * the latter is never actually used!)
 *
 * The target is currently not used. It is expected to be either 0
 * (for the window-system-defined framebuffer) or GL_FRAMEBUFFER_EXT
 * (for application-defined framebuffers). In the future it may be used
 * to allow other types of framebuffers e.g. pbuffers.
 */
static bool send_data_framebuffer(uint32_t id, GLuint fbo, GLenum target,
                                  GLenum buffer, GLenum format, GLenum type)
{
    Display *dpy = NULL;
    GLXContext aux = NULL, real = NULL;
    GLXDrawable old_read = 0, old_write = 0;
    pixel_state old_pack;
    GLint old_fbo = 0, old_read_buffer;
    GLint width, height;
    size_t length;
    char *data;
    bool illegal;

    if (!bugle_begin_internal_render())
    {
        gldb_protocol_send_code(out_pipe, RESP_ERROR);
        gldb_protocol_send_code(out_pipe, id);
        gldb_protocol_send_code(out_pipe, 0);
        gldb_protocol_send_string(out_pipe, "inside glBegin/glEnd");
        return false;
    }


#ifdef GL_EXT_framebuffer_object
    illegal = !bugle_gl_has_extension(BUGLE_GL_EXT_framebuffer_object) && fbo;
#else
    illegal = fbo != 0;
#endif
    if (illegal)
    {
        gldb_protocol_send_code(out_pipe, RESP_ERROR);
        gldb_protocol_send_code(out_pipe, id);
        gldb_protocol_send_code(out_pipe, 0);
        gldb_protocol_send_string(out_pipe, "GL_EXT_framebuffer_object is not available");
        return false;
    }

    if (fbo && (aux = bugle_get_aux_context()) != NULL)
    {
        real = CALL_glXGetCurrentContext();
        old_write = CALL_glXGetCurrentDrawable();
        old_read = CALL_glXGetCurrentReadDrawable();
        dpy = CALL_glXGetCurrentDisplay();
        CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);

        CALL_glPushAttrib(GL_ALL_ATTRIB_BITS);
        CALL_glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        CALL_glPixelStorei(GL_PACK_ALIGNMENT, 1);
    }
    else
    {
        pixel_pack_reset(&old_pack);
#ifdef GL_EXT_framebuffer_blit
        if (bugle_gl_has_extension(BUGLE_GL_EXT_framebuffer_blit))
            CALL_glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &old_fbo);
        else
#endif
        {
#ifdef GL_EXT_framebuffer_object
            if (bugle_gl_has_extension(BUGLE_GL_EXT_framebuffer_object))
                CALL_glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &old_fbo);
#endif
        }
    }

    if ((GLint) fbo != old_fbo)
    {
#ifdef GL_EXT_framebuffer_blit
        if (bugle_gl_has_extension(BUGLE_GL_EXT_framebuffer_blit))
            CALL_glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, fbo);
        else
#endif
        {
#ifdef GL_EXT_framebuffer_object
            if (bugle_gl_has_extension(BUGLE_GL_EXT_framebuffer_object))
                CALL_glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
        }
#endif
    }

    /* Note: GL_READ_BUFFER belongs to the FBO where an application-created
     * FBO is in use. Thus, we must save the old value even if we are
     * using the aux context. We're also messing with shared object state,
     * so if anyone was insane enough to be using the FBO for readback in
     * another thread at the same time, things might go pear-shaped. The
     * workaround would be to query all the bindings of the FBO and clone
     * a brand new FBO, but that seems like a lot of work and quite
     * fragile to future changes in the way FBO's are handled.
     */
    if (format != GL_DEPTH_COMPONENT && format != GL_STENCIL_INDEX)
    {
        glGetIntegerv(GL_READ_BUFFER, &old_read_buffer);
        glReadBuffer(buffer);
    }

    get_framebuffer_size(fbo, target, buffer, &width, &height);
    length = bugle_gl_type_to_size(type) * bugle_gl_format_to_count(format, type)
        * width * height;
    data = bugle_malloc(length);
    CALL_glReadPixels(0, 0, width, height, format, type, data);

    /* Restore the old state. glPushAttrib currently does NOT save FBO
     * bindings, so we have to do that manually even for the aux context. */
    if (format != GL_DEPTH_COMPONENT && format != GL_STENCIL_INDEX)
        glReadBuffer(old_read_buffer);
    if (aux && fbo)
    {
        GLenum error;
        while ((error = CALL_glGetError()) != GL_NO_ERROR)
            fprintf(stderr, "Warning: GL error %#04x generated by send_data_framebuffer in aux context\n", error);
#ifdef GL_EXT_framebuffer_blit
        if (bugle_gl_has_extension(BUGLE_GL_EXT_framebuffer_blit))
            CALL_glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
        else
#endif
        {
#ifdef GL_EXT_framebuffer_object
            /* No runtime check needed, since fbo != 0 */
            CALL_glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
#endif
        }
        CALL_glPopClientAttrib();
        CALL_glPopAttrib();
        CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
    }
    else
    {
        if ((GLint) fbo != old_fbo)
        {
#ifdef GL_EXT_framebuffer_blit
            if (bugle_gl_has_extension(BUGLE_GL_EXT_framebuffer_blit))
                CALL_glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, old_fbo);
            else
#endif
            {
#ifdef GL_EXT_framebuffer_object
                CALL_glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, old_fbo);
#endif
            }
        }
        pixel_pack_restore(&old_pack);
    }

    gldb_protocol_send_code(out_pipe, RESP_DATA);
    gldb_protocol_send_code(out_pipe, id);
    gldb_protocol_send_code(out_pipe, REQ_DATA_FRAMEBUFFER);
    gldb_protocol_send_binary_string(out_pipe, length, data);
    gldb_protocol_send_code(out_pipe, width);
    gldb_protocol_send_code(out_pipe, height);
    bugle_end_internal_render("send_data_framebuffer", true);
    free(data);
    return true;
}

#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program) || defined(GL_ARB_vertex_shader) || defined(GL_ARB_fragment_shader)
static bool send_data_shader(uint32_t id, GLuint shader_id,
                             GLenum target)
{
    Display *dpy;
    GLXContext aux, real;
    GLXDrawable old_read, old_write;
    GLenum error;
    GLint length;
    char *text;

    aux = bugle_get_aux_context();
    if (!aux || !bugle_begin_internal_render())
    {
        gldb_protocol_send_code(out_pipe, RESP_ERROR);
        gldb_protocol_send_code(out_pipe, id);
        gldb_protocol_send_code(out_pipe, 0);
        gldb_protocol_send_string(out_pipe, "inside glBegin/glEnd");
        return false;
    }

    real = CALL_glXGetCurrentContext();
    old_write = CALL_glXGetCurrentDrawable();
    old_read = CALL_glXGetCurrentReadDrawable();
    dpy = CALL_glXGetCurrentDisplay();
    CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);

    CALL_glPushAttrib(GL_ALL_ATTRIB_BITS);
    switch (target)
    {
#ifdef GL_ARB_fragment_program
    case GL_FRAGMENT_PROGRAM_ARB:
#endif
#ifdef GL_ARB_vertex_program
    case GL_VERTEX_PROGRAM_ARB:
#endif
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
        CALL_glBindProgramARB(target, shader_id);
        CALL_glGetProgramivARB(target, GL_PROGRAM_LENGTH_ARB, &length);
        text = bugle_malloc(length);
        CALL_glGetProgramStringARB(target, GL_PROGRAM_STRING_ARB, text);
        break;
#endif
#ifdef GL_ARB_vertex_shader
    case GL_VERTEX_SHADER_ARB:
#endif
#ifdef GL_ARB_fragment_shader
    case GL_FRAGMENT_SHADER_ARB:
#endif
#ifdef GL_ARB_shader_objects
        glsl_glGetShaderiv(shader_id, GL_OBJECT_SHADER_SOURCE_LENGTH_ARB, &length);
        if (length != 0)
        {
            text = bugle_malloc(length);
            glsl_glGetShaderSource(shader_id, length, NULL, (GLcharARB *) text);
            length--; /* Don't count NULL terminator */
        }
        else
            text = bugle_malloc(1);
#endif
        break;
    default:
        /* Should never get here */
        text = bugle_malloc(1);
        length = 0;
    }

    gldb_protocol_send_code(out_pipe, RESP_DATA);
    gldb_protocol_send_code(out_pipe, id);
    gldb_protocol_send_code(out_pipe, REQ_DATA_SHADER);
    gldb_protocol_send_binary_string(out_pipe, length, text);
    free(text);

    CALL_glPopAttrib();
    while ((error = CALL_glGetError()) != GL_NO_ERROR)
        fprintf(stderr, "Warning: GL error %#04x generated by send_data_shader in aux context\n", error);
    CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
    bugle_end_internal_render("send_data_shader", true);
    return true;
}
#endif

static void process_single_command(function_call *call)
{
    uint32_t req, id, req_val;
    char *req_str, *resp_str;
    bool activate;
    budgie_function func;
    filter_set *f;

    if (!gldb_protocol_recv_code(in_pipe, &req)) exit(1);
    if (!gldb_protocol_recv_code(in_pipe, &id)) exit(1);

    switch (req)
    {
    case REQ_RUN:
        if (req == REQ_RUN)
        {
            gldb_protocol_send_code(out_pipe, RESP_RUNNING);
            gldb_protocol_send_code(out_pipe, id);
        }
        /* Fall through */
    case REQ_CONT:
    case REQ_STEP:
        break_on_next = (req == REQ_STEP);
        stopped = false;
        start_id = id;
        break;
    case REQ_BREAK:
        gldb_protocol_recv_string(in_pipe, &req_str);
        gldb_protocol_recv_code(in_pipe, &req_val);
        func = find_function(req_str);
        if (func != NULL_FUNCTION)
        {
            gldb_protocol_send_code(out_pipe, RESP_ANS);
            gldb_protocol_send_code(out_pipe, id);
            gldb_protocol_send_code(out_pipe, 0);
            break_on[func] = req_val != 0;
        }
        else
        {
            gldb_protocol_send_code(out_pipe, RESP_ERROR);
            gldb_protocol_send_code(out_pipe, id);
            gldb_protocol_send_code(out_pipe, 0);
            bugle_asprintf(&resp_str, "Unknown function %s", req_str);
            gldb_protocol_send_string(out_pipe, resp_str);
            free(resp_str);
        }
        free(req_str);
        break;
    case REQ_BREAK_ERROR:
        gldb_protocol_recv_code(in_pipe, &req_val);
        break_on_error = req_val != 0;
        gldb_protocol_send_code(out_pipe, RESP_ANS);
        gldb_protocol_send_code(out_pipe, id);
        gldb_protocol_send_code(out_pipe, 0);
        break;
    case REQ_ACTIVATE_FILTERSET:
    case REQ_DEACTIVATE_FILTERSET:
        activate = (req == REQ_ACTIVATE_FILTERSET);
        gldb_protocol_recv_string(in_pipe, &req_str);
        f = bugle_get_filter_set_handle(req_str);
        if (!f)
        {
            bugle_asprintf(&resp_str, "Unknown filter-set %s", req_str);
            gldb_protocol_send_code(out_pipe, RESP_ERROR);
            gldb_protocol_send_code(out_pipe, id);
            gldb_protocol_send_code(out_pipe, 0);
            gldb_protocol_send_string(out_pipe, resp_str);
            free(resp_str);
        }
        else if (!strcmp(req_str, "debugger"))
        {
            gldb_protocol_send_code(out_pipe, RESP_ERROR);
            gldb_protocol_send_code(out_pipe, id);
            gldb_protocol_send_code(out_pipe, 0);
            gldb_protocol_send_string(out_pipe, "Cannot activate or deactivate the debugger");
        }
        else if (!bugle_filter_set_is_loaded(f))
        {
            bugle_asprintf(&resp_str, "Filter-set %s is not loaded; it must be loaded at program start",
                           req_str);
            gldb_protocol_send_code(out_pipe, RESP_ERROR);
            gldb_protocol_send_code(out_pipe, id);
            gldb_protocol_send_code(out_pipe, 0);
            gldb_protocol_send_string(out_pipe, resp_str);
            free(resp_str);
        }
        else if (bugle_filter_set_is_active(f) == activate)
        {
            bugle_asprintf(&resp_str, "Filter-set %s is already %s",
                           req_str, activate ? "active" : "inactive");
            gldb_protocol_send_code(out_pipe, RESP_ERROR);
            gldb_protocol_send_code(out_pipe, id);
            gldb_protocol_send_code(out_pipe, 0);
            gldb_protocol_send_string(out_pipe, resp_str);
            free(resp_str);
        }
        else
        {
            if (activate) bugle_activate_filter_set_deferred(f);
            else bugle_deactivate_filter_set_deferred(f);
            if (!bugle_filter_set_is_active(bugle_get_filter_set_handle("debugger")))
            {
                gldb_protocol_send_code(out_pipe, RESP_ERROR);
                gldb_protocol_send_code(out_pipe, id);
                gldb_protocol_send_code(out_pipe, 0);
                gldb_protocol_send_string(out_pipe, "Debugger was disabled; re-enabling");
                bugle_activate_filter_set(bugle_get_filter_set_handle("debugger"));
            }
            else
            {
                gldb_protocol_send_code(out_pipe, RESP_ANS);
                gldb_protocol_send_code(out_pipe, id);
                gldb_protocol_send_code(out_pipe, 0);
            }
        }
        free(req_str);
        break;
    case REQ_STATE_TREE:
        send_state(bugle_state_get_root(), id);
        break;
    case REQ_SCREENSHOT:
        if (!debugger_screenshot(out_pipe))
        {
            gldb_protocol_send_code(out_pipe, RESP_ERROR);
            gldb_protocol_send_code(out_pipe, id);
            gldb_protocol_send_code(out_pipe, 0);
            gldb_protocol_send_string(out_pipe, "Not able to call GL at this time");
        }
        break;
    case REQ_QUIT:
        exit(1);
    case REQ_DATA:
        {
            uint32_t subtype, tex_id, shader_id, target, face, level, format, type;
            uint32_t fbo_id, buffer;

            gldb_protocol_recv_code(in_pipe, &subtype);
            switch (subtype)
            {
            case REQ_DATA_TEXTURE:
                gldb_protocol_recv_code(in_pipe, &tex_id);
                gldb_protocol_recv_code(in_pipe, &target);
                gldb_protocol_recv_code(in_pipe, &face);
                gldb_protocol_recv_code(in_pipe, &level);
                gldb_protocol_recv_code(in_pipe, &format);
                gldb_protocol_recv_code(in_pipe, &type);
                send_data_texture(id, tex_id, target, face, level,
                                  format, type);
                break;
            case REQ_DATA_FRAMEBUFFER:
                gldb_protocol_recv_code(in_pipe, &fbo_id);
                gldb_protocol_recv_code(in_pipe, &target);
                gldb_protocol_recv_code(in_pipe, &buffer);
                gldb_protocol_recv_code(in_pipe, &format);
                gldb_protocol_recv_code(in_pipe, &type);
                send_data_framebuffer(id, fbo_id, target, buffer, format, type);
                break;
            case REQ_DATA_SHADER:
                gldb_protocol_recv_code(in_pipe, &shader_id);
                gldb_protocol_recv_code(in_pipe, &target);
                send_data_shader(id, shader_id, target);
                break;
            default:
                gldb_protocol_send_code(out_pipe, RESP_ERROR);
                gldb_protocol_send_code(out_pipe, id);
                gldb_protocol_send_code(out_pipe, 0);
                gldb_protocol_send_string(out_pipe, "unknown subtype");
                break;
            }
        }
        break;
    case REQ_ASYNC:
        if (!stopped)
        {
            resp_str = budgie_string_io(dump_any_call_string_io, call);
            stopped = true;
            gldb_protocol_send_code(out_pipe, RESP_STOP);
            gldb_protocol_send_code(out_pipe, start_id);
            gldb_protocol_send_string(out_pipe, resp_str);
            free(resp_str);
        }
        break;
    }
}

/* The main initialiser calls this with call == NULL. It has essentially
 * the usual effects, but refuses to do anything that doesn't make sense
 * until the program is properly running (such as flush or query state).
 */
static void debugger_loop(function_call *call)
{
    fd_set readfds, exceptfds;
    struct timeval timeout;
    int r;

    if (call && bugle_begin_internal_render())
    {
        CALL_glFinish();
        bugle_end_internal_render("debugger", true);
    }

    do
    {
        FD_ZERO(&readfds);
        FD_ZERO(&exceptfds);
        FD_SET(in_pipe, &readfds);
        FD_SET(in_pipe, &exceptfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        r = select(in_pipe + 1, &readfds, NULL, &exceptfds,
                   stopped ? NULL : &timeout);
        if (r == -1)
        {
            if (errno == EINTR) continue;
            else
            {
                perror("select");
                exit(1);
            }
        }
        if (FD_ISSET(in_pipe, &exceptfds))
        {
            fputs("bugle: exceptional condition on debugger input pipe\n", stderr);
            exit(1);
        }
        else if (FD_ISSET(in_pipe, &readfds))
            process_single_command(call);
    } while (stopped);
}

static bool debugger_callback(function_call *call, const callback_data *data)
{
    char *resp_str;

    if (break_on[call->generic.id])
    {
        resp_str = budgie_string_io(dump_any_call_string_io, call);
        stopped = true;
        break_on_next = false;
        gldb_protocol_send_code(out_pipe, RESP_BREAK);
        gldb_protocol_send_code(out_pipe, start_id);
        gldb_protocol_send_string(out_pipe, resp_str);
        free(resp_str);
    }
    else if (break_on_next)
    {
        resp_str = budgie_string_io(dump_any_call_string_io, call);
        break_on_next = false;
        stopped = true;
        gldb_protocol_send_code(out_pipe, RESP_STOP);
        gldb_protocol_send_code(out_pipe, start_id);
        gldb_protocol_send_string(out_pipe, resp_str);
        free(resp_str);
    }
    debugger_loop(call);
    return true;
}

static bool debugger_error_callback(function_call *call, const callback_data *data)
{
    GLenum error;
    char *resp_str;

    if (break_on_error
        && (error = bugle_get_call_error(call)))
    {
        resp_str = budgie_string_io(dump_any_call_string_io, call);
        gldb_protocol_send_code(out_pipe, RESP_BREAK_ERROR);
        gldb_protocol_send_code(out_pipe, start_id);
        gldb_protocol_send_string(out_pipe, resp_str);
        gldb_protocol_send_string(out_pipe, bugle_gl_enum_to_token(error));
        free(resp_str);
        stopped = true;
        debugger_loop(call);
    }
    return true;
}

static bool initialise_debugger(filter_set *handle)
{
    const char *env;
    char *last;
    filter *f;

    if (!getenv("BUGLE_DEBUGGER")
        || !getenv("BUGLE_DEBUGGER_FD_IN")
        || !getenv("BUGLE_DEBUGGER_FD_OUT"))
    {
        fputs("The debugger module should only be used with gldb\n", stderr);
        return false;
    }

    env = getenv("BUGLE_DEBUGGER_FD_IN");
    in_pipe = strtol(env, &last, 0);
    if (!*env || *last)
    {
        fprintf(stderr, "Illegal BUGLE_DEBUGGER_FD_IN: '%s' (bug in gldb?)",
                env);
        return false;
    }

    env = getenv("BUGLE_DEBUGGER_FD_OUT");
    out_pipe = strtol(env, &last, 0);
    if (!*env || *last)
    {
        fprintf(stderr, "Illegal BUGLE_DEBUGGER_FD_OUT: '%s' (bug in gldb?)",
                env);
        return false;
    }
    debugger_loop(NULL);

    f = bugle_register_filter(handle, "debugger");
    bugle_register_filter_catches_all(f, false, debugger_callback);
    f = bugle_register_filter(handle, "debugger_error");
    bugle_register_filter_catches_all(f, false, debugger_error_callback);
    bugle_register_filter_depends("invoke", "debugger");
    bugle_register_filter_depends("debugger_error", "invoke");
    bugle_register_filter_depends("debugger_error", "error");
    bugle_register_filter_post_renders("debugger_error");
    bugle_register_filter_set_queries_error("debugger");

    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info debugger_info =
    {
        "debugger",
        initialise_debugger,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        NULL /* no documentation */
    };

    memset(break_on, 0, sizeof(break_on));
    bugle_register_filter_set(&debugger_info);

    bugle_register_filter_set_depends("debugger", "error");
    bugle_register_filter_set_depends("debugger", "trackextensions");
    bugle_register_filter_set_depends("debugger", "trackobjects");
    bugle_register_filter_set_renders("debugger");
}
