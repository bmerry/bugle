/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008, 2012  Bruce Merry
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
#include <bugle/bool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <bugle/gl/glheaders.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glextensions.h>
#include <bugle/hashtable.h>
#include <bugle/filters.h>
#include <bugle/apireflect.h>
#include <bugle/input.h>
#include <bugle/log.h>
#include <bugle/string.h>
#include <bugle/memory.h>
#include <budgie/addresses.h>
#include <budgie/reflect.h>
#include "gl2ps/gl2ps.h"

typedef struct
{
    bugle_bool capture;            /* Set to BUGLE_TRUE when in feedback mode */
    size_t frame;
    FILE *stream;
} eps_struct;

static object_view eps_view;
static bugle_bool keypress_eps = BUGLE_FALSE;
static bugle_input_key key_eps = { BUGLE_INPUT_NOSYMBOL, 0, BUGLE_TRUE };
static char *eps_filename = NULL;
static char *eps_title = NULL;
static bugle_bool eps_bsp = BUGLE_FALSE;
static long eps_feedback_size = 0x100000;

static char *interpolate_filename(const char *pattern, int frame)
{
    if (strchr(pattern, '%'))
    {
        return bugle_asprintf(pattern, frame);
    }
    else
        return bugle_strdup(pattern);
}

static void eps_context_init(const void *key, void *data)
{
    eps_struct *d;

    d = (eps_struct *) data;
    d->frame = 0;
    d->stream = NULL;
}

static bugle_bool eps_swap_buffers(function_call *call, const callback_data *data)
{
    size_t frame;
    eps_struct *d;

    d = (eps_struct *) bugle_object_get_current_data(bugle_get_context_class(), eps_view);
    if (!d) return BUGLE_TRUE;
    frame = d->frame++;
    if (d->capture)
    {
        if (bugle_gl_begin_internal_render())
        {
            GLint status;

            status = gl2psEndPage();
            switch (status)
            {
            case GL2PS_OVERFLOW:
                bugle_log("eps", "gl2ps", BUGLE_LOG_NOTICE,
                          "Feedback buffer overflowed; size will be doubled (can be increased in configuration)");
                break;
            case GL2PS_NO_FEEDBACK:
                bugle_log("eps", "gl2ps", BUGLE_LOG_WARNING,
                          "No primitives were generated!");
                break;
            case GL2PS_UNINITIALIZED:
                bugle_log("eps", "gl2ps", BUGLE_LOG_WARNING,
                          "gl2ps was not initialised. This indicates a bug in bugle.");
                break;
            case GL2PS_ERROR:
                bugle_log("eps", "gl2ps", BUGLE_LOG_WARNING,
                          "An unknown gl2ps error occurred.");
                break;
            case GL2PS_SUCCESS:
                break;
            }
            fclose(d->stream);
            d->capture = BUGLE_FALSE;
            return BUGLE_FALSE; /* Don't swap, since it isn't a real frame */
        }
        else
            bugle_log("eps", "gl2ps", BUGLE_LOG_NOTICE,
                      "swap_buffers called inside glBegin/glEnd; snapshot may be corrupted.");
    }
    else if (keypress_eps && bugle_gl_begin_internal_render())
    {
        FILE *f;
        char *fname, *end;
        GLint format, status;
        GLfloat size;

        keypress_eps = BUGLE_FALSE;

        fname = interpolate_filename(eps_filename, frame);
        end = fname + strlen(fname);
        format = GL2PS_EPS;
        if (strlen(fname) >= 3 && !strcmp(end - 3, ".ps")) format = GL2PS_PS;
        if (strlen(fname) >= 4 && !strcmp(end - 4, ".eps")) format = GL2PS_EPS;
        if (strlen(fname) >= 4 && !strcmp(end - 4, ".pdf")) format = GL2PS_PDF;
        if (strlen(fname) >= 4 && !strcmp(end - 4, ".svg")) format = GL2PS_SVG;
        f = fopen(eps_filename, "wb");
        if (!f)
        {
            bugle_free(fname);
            bugle_log_printf("eps", "file", BUGLE_LOG_WARNING,
                             "Cannot open %s", eps_filename);
            return BUGLE_TRUE;
        }

        status = gl2psBeginPage(eps_title ? eps_title : "Unnamed scene", "bugle",
                                NULL, format,
                                eps_bsp ? GL2PS_BSP_SORT : GL2PS_SIMPLE_SORT,
                                GL2PS_NO_PS3_SHADING | GL2PS_OCCLUSION_CULL | GL2PS_USE_CURRENT_VIEWPORT,
                                GL_RGBA, 0, 0, 0, 0, 0, eps_feedback_size,
                                f, fname);
        if (status != GL2PS_SUCCESS)
        {
            bugle_log("eps", "gl2ps", BUGLE_LOG_WARNING,
                      "gl2psBeginPage failed");
            fclose(f);
            bugle_free(fname);
            return BUGLE_TRUE;
        }
        CALL(glGetFloatv)(GL_POINT_SIZE, &size);
        gl2psPointSize(size);
        CALL(glGetFloatv)(GL_LINE_WIDTH, &size);
        gl2psLineWidth(size);

        d->stream = f;
        d->capture = BUGLE_TRUE;
        bugle_free(fname);
        bugle_gl_end_internal_render("eps_swap_buffers", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool eps_glPointSize(function_call *call, const callback_data *data)
{
    eps_struct *d;

    d = (eps_struct *) bugle_object_get_current_data(bugle_get_context_class(), eps_view);
    if (d && d->capture && bugle_gl_begin_internal_render())
    {
        GLfloat size;
        CALL(glGetFloatv)(GL_POINT_SIZE, &size);
        gl2psPointSize(size);
        bugle_gl_end_internal_render("eps_glPointSize", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool eps_glLineWidth(function_call *call, const callback_data *data)
{
    eps_struct *d;

    d = (eps_struct *) bugle_object_get_current_data(bugle_get_context_class(), eps_view);
    if (d && d->capture && bugle_gl_begin_internal_render())
    {
        GLfloat width;
        CALL(glGetFloatv)(GL_LINE_WIDTH, &width);
        gl2psPointSize(width);
        bugle_gl_end_internal_render("eps_glLineWidth", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool eps_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "eps_pre");
    bugle_glwin_filter_catches_swap_buffers(f, BUGLE_FALSE, eps_swap_buffers);
    f = bugle_filter_new(handle, "eps");
    bugle_filter_catches(f, "glPointSize", BUGLE_FALSE, eps_glPointSize);
    bugle_filter_catches(f, "glLineWidth", BUGLE_FALSE, eps_glLineWidth);
    bugle_filter_order("eps_pre", "invoke");
    bugle_filter_order("invoke", "eps");
    bugle_gl_filter_post_renders("eps");
    eps_view = bugle_object_view_new(bugle_get_context_class(),
                                     eps_context_init,
                                     NULL,
                                     sizeof(eps_struct));
    bugle_input_key_callback(&key_eps, NULL, bugle_input_key_callback_flag, &keypress_eps);
    return BUGLE_TRUE;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info eps_variables[] =
    {
        { "filename", "file to write to (extension determines format) [bugle.eps]", FILTER_SET_VARIABLE_STRING, &eps_filename, NULL },
        { "key_eps", "key to trigger snapshot [C-A-S-W]", FILTER_SET_VARIABLE_KEY, &key_eps, NULL },
        { "title", "title in EPS file [Unnamed scene]", FILTER_SET_VARIABLE_STRING, &eps_title, NULL },
        { "bsp", "use BSP sorting (slower but more accurate) [no]", FILTER_SET_VARIABLE_BOOL, &eps_bsp, NULL },
        { "buffer", "feedback buffer size [1M]", FILTER_SET_VARIABLE_UINT, &eps_feedback_size, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info eps_info =
    {
        "eps",
        eps_initialise,
        NULL,
        NULL,
        NULL,
        eps_variables,
        "dumps scene to EPS/PS/PDF/SVG format"
    };

    eps_filename = bugle_strdup("bugle.eps");
    bugle_input_key_lookup("C-A-S-W", &key_eps);

    bugle_filter_set_new(&eps_info);

    bugle_gl_filter_set_renders("eps");
    bugle_filter_set_depends("eps", "trackcontext");
}
