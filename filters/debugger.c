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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _XOPEN_SOURCE
#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/tracker.h"
#include "src/glstate.h"
#include "src/glexts.h"
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
    int i;

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
    GLint width, height = 1, depth = 1;

    Display *dpy;
    GLXContext aux, real;
    GLXDrawable old_read, old_write;

    GLboolean old_pack_swap_bytes, old_pack_lsb_first;
    GLint old_pack_row_length, old_pack_skip_rows, old_pack_skip_pixels, old_pack_alignment;
#ifdef GL_EXT_texture3D
    GLint old_pack_image_height, old_pack_skip_images;
#endif
#ifdef GL_EXT_pixel_buffer_object
    GLint old_buffer;
#endif

    aux = bugle_get_aux_context();
    if (!aux || !bugle_begin_internal_render())
    {
        gldb_protocol_send_code(out_pipe, RESP_ERROR);
        gldb_protocol_send_code(out_pipe, id);
        gldb_protocol_send_code(out_pipe, 0);
        gldb_protocol_send_string(out_pipe, "inside glBegin/glEnd");
        return false;
    }

    if (texid)
    {
        real = CALL_glXGetCurrentContext();
        old_write = CALL_glXGetCurrentDrawable();
        old_read = CALL_glXGetCurrentReadDrawable();
        dpy = CALL_glXGetCurrentDisplay();
        CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    }
    else
    {
        CALL_glGetBooleanv(GL_PACK_SWAP_BYTES, &old_pack_swap_bytes);
        CALL_glGetBooleanv(GL_PACK_LSB_FIRST, &old_pack_lsb_first);
        CALL_glGetIntegerv(GL_PACK_ROW_LENGTH, &old_pack_row_length);
        CALL_glGetIntegerv(GL_PACK_SKIP_ROWS, &old_pack_skip_rows);
        CALL_glGetIntegerv(GL_PACK_SKIP_PIXELS, &old_pack_skip_pixels);
        CALL_glGetIntegerv(GL_PACK_ALIGNMENT, &old_pack_alignment);
        CALL_glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
        CALL_glPixelStorei(GL_PACK_LSB_FIRST, GL_FALSE);
        CALL_glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        CALL_glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        CALL_glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
#ifdef GL_EXT_texture3D
        if (bugle_gl_has_extension(BUGLE_GL_EXT_texture3D))
        {
            CALL_glGetIntegerv(GL_PACK_IMAGE_HEIGHT, &old_pack_image_height);
            CALL_glGetIntegerv(GL_PACK_SKIP_IMAGES, &old_pack_skip_images);
            CALL_glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
            CALL_glPixelStorei(GL_PACK_SKIP_IMAGES, 0);
        }
#endif
#ifdef GL_EXT_pixel_buffer_object
        if (bugle_gl_has_extension(BUGLE_GL_EXT_pixel_buffer_object))
        {
            CALL_glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING_EXT, &old_buffer);
            CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, 0);
        }
#endif
    }

    glBindTexture(target, texid);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glGetTexLevelParameteriv(face, level, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(face, level, GL_TEXTURE_HEIGHT, &height);
#ifdef GL_EXT_texture3D
    if (bugle_gl_has_extension(BUGLE_GL_EXT_texture3D))
        glGetTexLevelParameteriv(face, level, GL_TEXTURE_DEPTH_EXT, &depth);
#endif
    length = bugle_gl_type_to_size(type) * bugle_gl_format_to_count(format, type)
        * width * height * depth;
    data = bugle_malloc(length);
    glGetTexImage(target, level, format, type, data);

    if (texid)
    {
        while (glGetError());  /* FIXME: handle any errors */
        glPopClientAttrib();
        glPopAttrib();
        CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
    }
    else
    {
        CALL_glPixelStorei(GL_PACK_SWAP_BYTES, old_pack_swap_bytes);
        CALL_glPixelStorei(GL_PACK_LSB_FIRST, old_pack_lsb_first);
        CALL_glPixelStorei(GL_PACK_ROW_LENGTH, old_pack_row_length);
        CALL_glPixelStorei(GL_PACK_SKIP_ROWS, old_pack_skip_rows);
        CALL_glPixelStorei(GL_PACK_SKIP_PIXELS, old_pack_skip_pixels);
        CALL_glPixelStorei(GL_PACK_ALIGNMENT, old_pack_alignment);
#ifdef GL_EXT_texture3D
        if (bugle_gl_has_extension(BUGLE_GL_EXT_texture3D))
        {
            CALL_glPixelStorei(GL_PACK_IMAGE_HEIGHT, old_pack_image_height);
            CALL_glPixelStorei(GL_PACK_SKIP_IMAGES, old_pack_skip_images);
        }
#endif
#ifdef GL_EXT_pixel_buffer_object
        if (bugle_gl_has_extension(BUGLE_GL_EXT_pixel_buffer_object))
            CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, old_buffer);
#endif
    }

    gldb_protocol_send_code(out_pipe, RESP_DATA);
    gldb_protocol_send_code(out_pipe, id);
    gldb_protocol_send_code(out_pipe, REQ_DATA_TEXTURE);
    gldb_protocol_send_binary_string(out_pipe, length, data);
    gldb_protocol_send_code(out_pipe, width);
    gldb_protocol_send_code(out_pipe, height);
    gldb_protocol_send_code(out_pipe, depth);
    bugle_end_internal_render("send_data_texture", true);
    return true;
}

static void process_single_command(function_call *call)
{
    uint32_t req, id, req_val;
    char *req_str, *resp_str;
    bool enable;
    budgie_function func;
    filter_set *f;

    /* FIXME: error checking on the network code */
    gldb_protocol_recv_code(in_pipe, &req);
    gldb_protocol_recv_code(in_pipe, &id);

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
    case REQ_ENABLE_FILTERSET:
    case REQ_DISABLE_FILTERSET:
        enable = (req == REQ_ENABLE_FILTERSET);
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
        else if (bugle_filter_set_is_enabled(f) == enable)
        {
            bugle_asprintf(&resp_str, "Filter-set %s is already %s",
                           req_str, enable ? "enabled" : "disabled");
            gldb_protocol_send_code(out_pipe, RESP_ERROR);
            gldb_protocol_send_code(out_pipe, id);
            gldb_protocol_send_code(out_pipe, 0);
            gldb_protocol_send_string(out_pipe, resp_str);
            free(resp_str);
        }
        else
        {
            if (enable) bugle_enable_filter_set(f);
            else bugle_disable_filter_set(f);
            if (!bugle_filter_set_is_enabled(bugle_get_filter_set_handle("debugger")))
            {
                gldb_protocol_send_code(out_pipe, RESP_ERROR);
                gldb_protocol_send_code(out_pipe, id);
                gldb_protocol_send_code(out_pipe, 0);
                gldb_protocol_send_string(out_pipe, "Debugger was disabled; re-enabling");
                bugle_enable_filter_set(bugle_get_filter_set_handle("debugger"));
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
            uint32_t subtype, texid, target, face, level, format, type;

            gldb_protocol_recv_code(in_pipe, &subtype);
            switch (subtype)
            {
            case REQ_DATA_TEXTURE:
                gldb_protocol_recv_code(in_pipe, &texid);
                gldb_protocol_recv_code(in_pipe, &target);
                gldb_protocol_recv_code(in_pipe, &face);
                gldb_protocol_recv_code(in_pipe, &level);
                gldb_protocol_recv_code(in_pipe, &format);
                gldb_protocol_recv_code(in_pipe, &type);
                send_data_texture(id, texid, target, face, level,
                                  format, type);
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
            resp_str[strlen(resp_str) - 1] = '\0'; /* strip the \n */
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
        resp_str[strlen(resp_str) - 1] = '\0'; /* strip the \n */
        stopped = true;
        gldb_protocol_send_code(out_pipe, RESP_BREAK);
        gldb_protocol_send_code(out_pipe, start_id);
        gldb_protocol_send_string(out_pipe, resp_str);
        free(resp_str);
    }
    else if (break_on_next)
    {
        resp_str = budgie_string_io(dump_any_call_string_io, call);
        resp_str[strlen(resp_str) - 1] = '\0'; /* strip the \n */
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
        resp_str[strlen(resp_str) - 1] = '\0'; /* strip the \n */
        gldb_protocol_send_code(out_pipe, RESP_BREAK_ERROR);
        gldb_protocol_send_code(out_pipe, start_id);
        gldb_protocol_send_string(out_pipe, resp_str);
        gldb_protocol_send_string(out_pipe, bugle_gl_enum_to_token(error));
        free(resp_str);
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
    bugle_register_filter_catches_all(f, debugger_callback);
    f = bugle_register_filter(handle, "debugger_error");
    bugle_register_filter_catches_all(f, debugger_error_callback);
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
