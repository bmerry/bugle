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
#include <bugle/gl/glheaders.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glextensions.h>
#include <bugle/linkedlist.h>
#include <bugle/misc.h>
#include <bugle/stats.h>
#include <bugle/filters.h>
#include <bugle/log.h>
#include <bugle/input.h>
#include <budgie/addresses.h>
#include "xalloc.h"

typedef struct
{
    struct timeval last_update;
    int accumulating;  /* 0: no  1: yes  2: yes, reset counters */

    stats_signal_values showstats_prev, showstats_cur;
    char *showstats_display;
    size_t showstats_display_size;
} showstats_struct;

typedef enum
{
    SHOWSTATS_TEXT,
    SHOWSTATS_GRAPH
} showstats_mode;

typedef struct
{
    showstats_mode mode;
    stats_statistic *st;
    bool initialised;

    /* Graph-specific stuff */
    double graph_scale;     /* Largest value on graph */
    char graph_scale_label[64]; /* Text equivalent of graph_scale */
    GLsizei graph_size;     /* Number of history samples */
    double *graph_history;  /* Raw (unscaled) history values */
    GLubyte *graph_scaled;  /* Scaled according to graph_scale for OpenGL */
    int graph_offset;       /* place to put next sample */

    GLuint graph_tex;       /* 1D texture to determine graph height */
} showstats_statistic;

typedef struct
{
    showstats_mode mode;
    char *name;
} showstats_statistic_request; /* Items in the config file */

static object_view showstats_view;
static linked_list showstats_stats;  /* List of showstats_statistic */
static int showstats_num_graph;
static bugle_input_key key_showstats_accumulate = { BUGLE_INPUT_NOSYMBOL, 0, true };
static bugle_input_key key_showstats_noaccumulate = { BUGLE_INPUT_NOSYMBOL, 0, true };
static double showstats_time = 0.2;

static linked_list showstats_stats_requested;

static double time_elapsed(struct timeval *old, struct timeval *now)
{
    return (now->tv_sec - old->tv_sec) + 1e-6 * (now->tv_usec - old->tv_usec);
}

/* Creates the extended data (e.g. OpenGL objects) that depend on having
 * the aux context active.
 */
static void showstats_statistic_initialise(showstats_statistic *sst)
{
    if (sst->initialised) return;
    switch (sst->mode)
    {
    case SHOWSTATS_TEXT:
        sst->initialised = true;
        break;
    case SHOWSTATS_GRAPH:
#ifdef GL_ARB_texture_env_combine
        if (!BUGLE_GL_HAS_EXTENSION(GL_ARB_texture_env_combine))
#endif
        {
            bugle_log("showstats", "graph", BUGLE_LOG_ERROR,
                      "Graphing currently requires GL_ARB_texture_env_combine.");
            exit(1);
        }
#ifdef GL_ARB_texture_env_combine
        else if (bugle_gl_begin_internal_render())
        {
            GLint max_size;

            showstats_num_graph++;
            sst->graph_size = 128;
            CALL(glGetIntegerv)(GL_MAX_TEXTURE_SIZE, &max_size);
            if (max_size < sst->graph_size)
                sst->graph_size = max_size;
            sst->graph_history = XCALLOC(sst->graph_size, double);
            sst->graph_scaled = XCALLOC(sst->graph_size, GLubyte);
            sst->graph_scale = sst->st->maximum;
            sst->graph_scale_label[0] = '\0';

            CALL(glGenTextures)(1, &sst->graph_tex);
            CALL(glBindTexture)(GL_TEXTURE_1D, sst->graph_tex);
            CALL(glTexParameteri)(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            CALL(glTexParameteri)(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            CALL(glTexParameteri)(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            CALL(glTexParameteri)(GL_TEXTURE_1D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
            CALL(glTexImage1D)(GL_TEXTURE_1D, 0, GL_ALPHA8,
                              sst->graph_size, 0, GL_ALPHA, GL_UNSIGNED_BYTE, sst->graph_scaled);
            CALL(glBindTexture)(GL_TEXTURE_1D, 0);
            bugle_gl_end_internal_render("showstats_statistic_initialise", true);
            sst->initialised = true;
        }
#endif
        break;
    }
}

static void showstats_graph_rescale(showstats_statistic *sst, double new_scale)
{
    double p10 = 1.0;
    double s = 0.0, v;
    int i, e = 0;
    /* Find a suitably large, "nice" value */

    while (new_scale > 5.0)
    {
        p10 *= 10.0;
        new_scale /= 10.0;
        e++;
    }
    if (new_scale <= 1.0) s = p10;
    else if (new_scale <= 2.0) s = 2.0 * p10;
    else s = 5.0 * p10;

    if (e > 11)
        snprintf(sst->graph_scale_label, sizeof(sst->graph_scale_label),
                 "%.0e", s);
    else if (e >= 9)
        snprintf(sst->graph_scale_label, sizeof(sst->graph_scale_label),
                 "%.0fG", s * 1e-9);
    else if (e >= 6)
        snprintf(sst->graph_scale_label, sizeof(sst->graph_scale_label),
                 "%.0fM", s * 1e-6);
    else if (e >= 3)
        snprintf(sst->graph_scale_label, sizeof(sst->graph_scale_label),
                 "%.0fK", s * 1e-3);
    else
        snprintf(sst->graph_scale_label, sizeof(sst->graph_scale_label),
                 "%.0f", s);

    sst->graph_scale = s;
    /* Regenerate the texture at the new scale */
    for (i = 0; i < sst->graph_size; i++)
    {
        v = sst->graph_history[i] >= 0.0 ? sst->graph_history[i] : 0.0;
        sst->graph_scaled[i] = (GLubyte) rint(v * 255.0 / s);
    }
    CALL(glBindTexture)(GL_TEXTURE_1D, sst->graph_tex);
    CALL(glTexSubImage1D)(GL_TEXTURE_1D, 0, 0, sst->graph_size,
                         GL_ALPHA, GL_UNSIGNED_BYTE, sst->graph_scaled);
    CALL(glBindTexture)(GL_TEXTURE_1D, 0);
}

static void showstats_update(showstats_struct *ss)
{
    struct timeval now;
    linked_list_node *i;
    showstats_statistic *sst;
    stats_substitution *sub;
    double v;

    gettimeofday(&now, NULL);
    if (time_elapsed(&ss->last_update, &now) >= showstats_time)
    {
        ss->last_update = now;
        bugle_stats_signal_values_gather(&ss->showstats_cur);

        if (ss->showstats_prev.allocated)
        {
            if (ss->showstats_display) ss->showstats_display[0] = '\0';
            for (i = bugle_list_head(&showstats_stats); i; i = bugle_list_next(i))
            {
                sst = (showstats_statistic *) bugle_list_data(i);
                if (!sst->initialised) showstats_statistic_initialise(sst);
                v = bugle_stats_expression_evaluate(sst->st->value, &ss->showstats_prev, &ss->showstats_cur);
                switch (sst->mode)
                {
                case SHOWSTATS_TEXT:
                    if (FINITE(v))
                    {
                        sub = bugle_stats_statistic_find_substitution(sst->st, v);
                        if (sub)
                            bugle_appendf(&ss->showstats_display, &ss->showstats_display_size,
                                          "%10s %s\n", sub->replacement, sst->st->label);
                        else
                            bugle_appendf(&ss->showstats_display, &ss->showstats_display_size,
                                          "%10.*f %s\n", sst->st->precision, v, sst->st->label);
                    }
                    break;
                case SHOWSTATS_GRAPH:
                    if (sst->graph_tex)
                    {
                        GLubyte vs;

                        if (!FINITE(v)) v = 0.0;
                        sst->graph_history[sst->graph_offset] = v;
                        /* Check if we need to rescale */
                        if (v > sst->graph_scale)
                            showstats_graph_rescale(sst, v);
                        v /= sst->graph_scale;
                        if (v < 0.0) v = 0.0;
                        vs = (GLubyte) rint(v * 255.0);
                        CALL(glBindTexture)(GL_TEXTURE_1D, sst->graph_tex);
                        CALL(glTexSubImage1D)(GL_TEXTURE_1D, 0, sst->graph_offset, 1, GL_ALPHA, GL_UNSIGNED_BYTE, &vs);
                        CALL(glBindTexture)(GL_TEXTURE_1D, 0);

                        sst->graph_scaled[sst->graph_offset] = vs;
                        sst->graph_offset++;
                        if (sst->graph_offset >= sst->graph_size)
                            sst->graph_offset = 0;
                    }
                    break;
                }
            }
        }
        if (ss->accumulating != 1 || !ss->showstats_prev.allocated)
        {
            /* Bring prev up to date for next time. Swap so that we recycle
             * memory for next time.
             */
            stats_signal_values tmp;
            tmp = ss->showstats_prev;
            ss->showstats_prev = ss->showstats_cur;
            ss->showstats_cur = tmp;
        }
        if (ss->accumulating == 2) ss->accumulating = 1;
    }
}

/* Helper function to apply one pass of the graph-drawing to all graphs.
 * If graph_tex is true, the graph texture is bound before each draw
 * call. If graph_texcoords is not NULL, it is populated with the
 * appropriate texture coordinates before each draw call.
 */
static void showstats_graph_draw(GLenum mode, int xofs0, int yofs0,
                                 bool graph_tex, GLfloat *graph_texcoords)
{
    showstats_statistic *sst;
    linked_list_node *i;
    int xofs, yofs;

    xofs = xofs0;
    yofs = yofs0;
    for (i = bugle_list_head(&showstats_stats); i; i = bugle_list_next(i))
    {
        sst = (showstats_statistic *) bugle_list_data(i);
        if (sst->mode == SHOWSTATS_GRAPH && sst->graph_tex)
        {
            if (graph_texcoords)
            {
                GLfloat s1, s2;

                s1 = (sst->graph_offset + 0.5f) / sst->graph_size;
                s2 = (sst->graph_offset - 0.5f) / sst->graph_size + 1.0f;
                graph_texcoords[0] = s1;
                graph_texcoords[1] = s2;
                graph_texcoords[2] = s2;
                graph_texcoords[3] = s1;
            }
            CALL(glPushMatrix)();
            CALL(glTranslatef)(xofs, yofs, 0.0f);
            CALL(glScalef)(sst->graph_size, 32.0f, 1.0f);
            if (graph_tex)
                CALL(glBindTexture)(GL_TEXTURE_1D, sst->graph_tex);
            CALL(glDrawArrays)(mode, 0, 4);
            CALL(glPopMatrix)();
            yofs -= 64;
        }
    }
}

static bool showstats_swap_buffers(function_call *call, const callback_data *data)
{
    glwin_display dpy;
    glwin_drawable old_read, old_write;
    glwin_context aux, real;
    showstats_struct *ss;
    GLint viewport[4];
    linked_list_node *i;
    showstats_statistic *sst;
    int xofs, yofs, xofs0, yofs0;

    /* Canonical rectangle - scaled by matrix transforms */
    GLfloat graph_vertices[8] =
    {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };
    GLfloat graph_texcoords[4];
    GLubyte graph_colors[16] =
    {
        0, 255, 0, 0,
        0, 255, 0, 0,
        0, 255, 0, 255,
        0, 255, 0, 255
    };

    ss = bugle_object_get_current_data(bugle_context_class, showstats_view);
    aux = bugle_get_aux_context(false);
    if (aux && bugle_gl_begin_internal_render())
    {
        CALL(glGetIntegerv)(GL_VIEWPORT, viewport);
        real = bugle_glwin_get_current_context();
        old_write = bugle_glwin_get_current_drawable();
        old_read = bugle_glwin_get_current_read_drawable();
        dpy = bugle_glwin_get_current_display();
        bugle_glwin_make_context_current(dpy, old_write, old_write, aux);

        showstats_update(ss);

        CALL(glPushAttrib)(GL_CURRENT_BIT | GL_VIEWPORT_BIT);
        /* Expand viewport to whole window */
        CALL(glViewport)(viewport[0], viewport[1], viewport[2], viewport[3]);
        /* Make coordinates correspond to pixel offsets */
        CALL(glTranslatef)(-1.0f, -1.0f, 0.0f);
        CALL(glScalef)(2.0f / viewport[2], 2.0f / viewport[3], 1.0f);

        if (ss->showstats_display)
            bugle_gl_text_render(ss->showstats_display, 16, viewport[3] - 16);

#ifdef GL_ARB_texture_env_combine
        if (showstats_num_graph)
        {
            /* The graph drawing is done in several passes, to avoid multiple
             * state changes.
             */
            xofs0 = 16;
            yofs0 = 16 + 64 * (showstats_num_graph - 1);

            /* Common state to first several passes */
            CALL(glAlphaFunc)(GL_GREATER, 0.0f);
            CALL(glVertexPointer)(2, GL_FLOAT, 0, graph_vertices);
            CALL(glTexCoordPointer)(1, GL_FLOAT, 0, graph_texcoords);
            CALL(glColorPointer)(4, GL_UNSIGNED_BYTE, 0, graph_colors);
            CALL(glEnableClientState)(GL_VERTEX_ARRAY);

            /* Pass 1: clear the background */
            CALL(glColor3f)(0.0f, 0.0f, 0.0f);
            showstats_graph_draw(GL_QUADS, xofs0, yofs0, false, NULL);

            /* Pass 2: draw the graphs */
            CALL(glEnable)(GL_ALPHA_TEST);
            CALL(glEnable)(GL_TEXTURE_1D);
            CALL(glEnableClientState)(GL_COLOR_ARRAY);
            CALL(glEnableClientState)(GL_TEXTURE_COORD_ARRAY);
            CALL(glTexEnvi)(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
            CALL(glTexEnvi)(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS);
            CALL(glTexEnvi)(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE);
            CALL(glTexEnvi)(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_SUBTRACT_ARB);
            showstats_graph_draw(GL_QUADS, xofs0, yofs0, true, graph_texcoords);
            CALL(glTexEnvi)(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            CALL(glTexEnvi)(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
            CALL(glTexEnvi)(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
            CALL(glTexEnvi)(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
            CALL(glDisableClientState)(GL_TEXTURE_COORD_ARRAY);
            CALL(glDisableClientState)(GL_COLOR_ARRAY);
            CALL(glDisable)(GL_TEXTURE_1D);
            CALL(glDisable)(GL_ALPHA_TEST);
            CALL(glBindTexture)(GL_TEXTURE_1D, 0);

            /* Pass 3: draw the border */
            CALL(glColor3f)(1.0f, 1.0f, 1.0f);
            showstats_graph_draw(GL_LINE_LOOP, xofs0, yofs0, false, NULL);

            /* Pass 4: labels */
            xofs = xofs0;
            yofs = yofs0;
            for (i = bugle_list_head(&showstats_stats); i; i = bugle_list_next(i))
            {
                sst = (showstats_statistic *) bugle_list_data(i);
                if (sst->mode == SHOWSTATS_GRAPH && sst->graph_tex)
                {
                    bugle_gl_text_render(sst->st->label, xofs, yofs + 48);
                    bugle_gl_text_render(sst->graph_scale_label, xofs + sst->graph_size + 2, yofs + 40);
                    yofs -= 64;
                }
            }

            /* Clean up */
            CALL(glDisableClientState)(GL_VERTEX_ARRAY);
            CALL(glVertexPointer)(4, GL_FLOAT, 0, NULL);
            CALL(glTexCoordPointer)(4, GL_FLOAT, 0, NULL);
            CALL(glColorPointer)(4, GL_FLOAT, 0, NULL);
            CALL(glAlphaFunc)(GL_ALWAYS, 0.0f);
        }
#endif
        CALL(glLoadIdentity)();
        CALL(glPopAttrib)();

        bugle_glwin_make_context_current(dpy, old_write, old_read, real);
        bugle_gl_end_internal_render("showstats_callback", true);
    }
    return true;
}

static void showstats_accumulate_callback(const bugle_input_key *key, void *arg, bugle_input_event *event)
{
    showstats_struct *ss;
    ss = bugle_object_get_current_data(bugle_context_class, showstats_view);
    if (!ss) return;
    /* Value of arg is irrelevant, only truth value */
    ss->accumulating = arg ? 2 : 0;
}

/* Callback to assign the "show" pseudo-variable */
static bool showstats_show_set(const struct filter_set_variable_info_s *var,
                               const char *text, const void *value)
{
    showstats_statistic_request *req;

    req = XMALLOC(showstats_statistic_request);
    req->name = xstrdup(text);
    req->mode = SHOWSTATS_TEXT;
    bugle_list_append(&showstats_stats_requested, req);
    return true;
}

/* Same as above but for graphing */
static bool showstats_graph_set(const struct filter_set_variable_info_s *var,
                                const char *text, const void *value)
{
    showstats_statistic_request *req;

    req = XMALLOC(showstats_statistic_request);
    req->name = xstrdup(text);
    req->mode = SHOWSTATS_GRAPH;
    bugle_list_append(&showstats_stats_requested, req);
    return true;
}

static void showstats_struct_clear(void *data)
{
    showstats_struct *ss;

    ss = (showstats_struct *) data;
    bugle_stats_signal_values_clear(&ss->showstats_prev);
    bugle_stats_signal_values_clear(&ss->showstats_cur);
    free(ss->showstats_display);
}

static bool showstats_initialise(filter_set *handle)
{
    filter *f;
    linked_list_node *i;

    f = bugle_filter_new(handle, "showstats");
    bugle_filter_order("showstats", "invoke");
    bugle_filter_order("showstats", "screenshot");
    /* make sure that screenshots capture the stats */
    bugle_filter_order("showstats", "debugger");
    bugle_filter_order("showstats", "screenshot");
    bugle_filter_order("stats", "showstats");
    bugle_glwin_filter_catches_swap_buffers(f, false, showstats_swap_buffers);
    showstats_view = bugle_object_view_new(bugle_context_class,
                                           NULL,
                                           showstats_struct_clear,
                                           sizeof(showstats_struct));

    /* Value of arg is irrelevant, only truth value */
    bugle_input_key_callback(&key_showstats_accumulate, NULL,
                             showstats_accumulate_callback, f);
    bugle_input_key_callback(&key_showstats_noaccumulate, NULL,
                             showstats_accumulate_callback, NULL);

    showstats_num_graph = 0;
    for (i = bugle_list_head(&showstats_stats_requested); i; i = bugle_list_next(i))
    {
        showstats_statistic_request *req;
        showstats_statistic *sst;
        linked_list_node *j;

        req = (showstats_statistic_request *) bugle_list_data(i);
        j = bugle_stats_statistic_find(req->name);
        if (!j)
        {
            bugle_log_printf("showstats", "initialise", BUGLE_LOG_ERROR,
                             "statistic '%s' not found.", req->name);
            bugle_stats_statistic_list();
            return false;
        }
        for (; j; j = bugle_list_next(j))
        {
            sst = XZALLOC(showstats_statistic);
            sst->st = (stats_statistic *) bugle_list_data(j);
            sst->mode = req->mode;
            if (!bugle_stats_expression_activate_signals(sst->st->value))
            {
                bugle_log_printf("showstats", "initialise", BUGLE_LOG_ERROR,
                                 "could not initialise statistic '%s'",
                                 sst->st->name);
                return false;
            }
            bugle_list_append(&showstats_stats, sst);
            if (sst->st->last) break;
        }
    }

    return true;
}

static void showstats_shutdown(filter_set *handle)
{
    linked_list_node *i;
    showstats_statistic_request *req;

    for (i = bugle_list_head(&showstats_stats_requested); i; i = bugle_list_next(i))
    {
        req = (showstats_statistic_request *) bugle_list_data(i);
        free(req->name);
    }
    bugle_list_clear(&showstats_stats_requested);
    bugle_list_clear(&showstats_stats);
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info showstats_variables[] =
    {
        { "show", "repeat with each item to render", FILTER_SET_VARIABLE_CUSTOM, NULL, showstats_show_set },
        { "graph", "repeat with each item to graph", FILTER_SET_VARIABLE_CUSTOM, NULL, showstats_graph_set },
        { "key_accumulate", "frame rate is averaged from time key is pressed [none]", FILTER_SET_VARIABLE_KEY, &key_showstats_accumulate, NULL },
        { "key_noaccumulate", "return frame rate to instantaneous display [none]", FILTER_SET_VARIABLE_KEY, &key_showstats_noaccumulate, NULL },
        { "time", "time interval between updates", FILTER_SET_VARIABLE_FLOAT, &showstats_time, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info showstats_info =
    {
        "showstats",
        showstats_initialise,
        showstats_shutdown,
        NULL,
        NULL,
        showstats_variables,
        "renders statistics onto the screen"
    };

    bugle_filter_set_new(&showstats_info);
    bugle_filter_set_depends("showstats", "glextensions");
    bugle_gl_filter_set_renders("showstats");
    bugle_filter_set_stats_logger("showstats");
    bugle_list_init(&showstats_stats_requested, free);
    bugle_list_init(&showstats_stats, free);
}
