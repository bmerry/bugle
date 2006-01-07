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
#ifndef GETTEXT_PACKAGE
# define GETTEXT_PACKAGE "gtk20"
#endif
#if HAVE_GTKGLEXT
# include "glee/GLee.h"
#endif
/* FIXME: Not sure if this the correct definition of GETTEXT_PACKAGE... */
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#if HAVE_GTKGLEXT
# include <gtk/gtkgl.h>
#endif
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include "common/protocol.h"
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include "src/names.h"
#include "gldb/gldb-common.h"

#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
# define GLDB_GUI_SHADER_OLD
#endif
#if defined(GL_ARB_vertex_shader) || defined(GL_ARB_fragment_shader)
# define GLDB_GUI_SHADER_GLSL_ARB
#endif
#ifdef GL_VERSION_2_0
# define GLDB_GUI_SHADER_GLSL_2_0
#endif
#if defined(GLDB_GUI_SHADER_OLD) || defined(GLDB_GUI_SHADER_GLSL_ARB) || defined(GLDB_GUI_SHADER_GLSL_2_0)
# define GLDB_GUI_SHADER
#endif

enum
{
    COLUMN_STATE_NAME,
    COLUMN_STATE_VALUE
};

enum
{
    COLUMN_BREAKPOINTS_FUNCTION
};

#ifdef HAVE_GTKGLEXT
enum
{
    COLUMN_TEXTURE_ID_ID,
    COLUMN_TEXTURE_ID_TARGET,
    COLUMN_TEXTURE_ID_LEVELS,
    COLUMN_TEXTURE_ID_TEXT
};

enum
{
    COLUMN_TEXTURE_LEVEL_VALUE,
    COLUMN_TEXTURE_LEVEL_TEXT
};

enum
{
    COLUMN_TEXTURE_ZOOM_VALUE,
    COLUMN_TEXTURE_ZOOM_TEXT,
    COLUMN_TEXTURE_ZOOM_SENSITIVE
};

enum
{
    COLUMN_TEXTURE_FILTER_VALUE,
    COLUMN_TEXTURE_FILTER_TEXT,
    COLUMN_TEXTURE_FILTER_TRUE,
    COLUMN_TEXTURE_FILTER_NON_MIP
};
#endif

struct GldbWindow;

typedef struct
{
    guint32 id;
    gboolean (*callback)(struct GldbWindow *, gldb_response *, gpointer user_data);
    gpointer user_data;
} response_handler;

typedef struct
{
    bool dirty;
    GtkWidget *page;
    GtkTreeStore *state_store;
} GldbWindowState;

#if HAVE_GTKGLEXT
typedef struct
{
    bool dirty;
    GtkWidget *page;
    GtkWidget *draw;
    GtkWidget *id, *level, *zoom;
    GtkToolItem *zoom_in, *zoom_out, *zoom_100;
    GtkWidget *mag_filter, *min_filter;

    GtkCellRenderer *min_filter_cell;

    GLenum display_target; /* GL_NONE if no texture is selected */
    GLint width, height;
    GLint max_viewport_dims[2];
} GldbWindowTexture;
#endif

typedef struct
{
    bool dirty;
    GtkWidget *page;
    GtkWidget *target, *id;
    GtkWidget *source;
} GldbWindowShader;

typedef struct
{
    bool dirty;
    GtkWidget *page;
    GtkListStore *backtrace_store;
} GldbWindowBacktrace;

typedef struct GldbWindow
{
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *statusbar;
    guint statusbar_context_id;
    GtkActionGroup *actions;
    GtkActionGroup *running_actions;
    GtkActionGroup *stopped_actions;
    GtkActionGroup *dead_actions;

    GIOChannel *channel;
    guint channel_watch;
    bugle_linked_list response_handlers;

    GldbWindowState state;
#if HAVE_GTKGLEXT
    GldbWindowTexture texture;
#endif
    GldbWindowShader shader;
    GldbWindowBacktrace backtrace;

    GtkListStore *breakpoints_store;
} GldbWindow;

typedef struct
{
    GldbWindow *parent;
    GtkWidget *dialog, *list;
} GldbBreakpointsDialog;

#if HAVE_GTKGLEXT
# define TEXTURE_CALLBACK_FLAG_FIRST 1
# define TEXTURE_CALLBACK_FLAG_LAST 2

typedef struct
{
    GLenum target;
    GLenum face;
    GLuint level;
    guint32 flags;
} texture_callback_data;

static GtkTreeModel *texture_mag_filters, *texture_min_filters;

#endif

static GtkListStore *function_names;
static guint32 seq = 0;

static void set_response_handler(GldbWindow *context, guint32 id,
                                 gboolean (*callback)(GldbWindow *, gldb_response *r, gpointer user_data),
                                 gpointer user_data)
{
    response_handler *h;

    h = bugle_malloc(sizeof(response_handler));
    h->id = id;
    h->callback = callback;
    h->user_data = user_data;
    bugle_list_append(&context->response_handlers, h);
}

/* We can't just rip out all the old state and plug in the new, because
 * that loses any expansions that may have been active. Instead, we
 * take each state in the store and try to match it with state in the
 * gldb state tree. If it fails, we delete the state. Any new state also
 * has to be placed in the correct position.
 */
static void update_state_r(const gldb_state *root, GtkTreeStore *store,
                           GtkTreeIter *parent)
{
    bugle_hash_table lookup;
    GtkTreeIter iter, iter2;
    gboolean valid;
    bugle_list_node *i;
    const gldb_state *child;
    gchar *name;

    bugle_hash_init(&lookup, false);

    /* Build lookup table */
    for (i = bugle_list_head(&root->children); i; i = bugle_list_next(i))
    {
        child = (const gldb_state *) bugle_list_data(i);
        if (child->name) bugle_hash_set(&lookup, child->name, (void *) child);
    }

    /* Update common, and remove items from store not present in state */
    valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &iter, parent);
    while (valid)
    {
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COLUMN_STATE_NAME, &name, -1);
        child = (const gldb_state *) bugle_hash_get(&lookup, name);
        g_free(name);
        if (child)
        {
            gchar *value;
            gchar *value_utf8;

            value = child->value ? child->value : "";
            value_utf8 = g_convert(value, -1, "UTF8", "ASCII", NULL, NULL, NULL);
            gtk_tree_store_set(store, &iter, COLUMN_STATE_VALUE, value_utf8 ? value_utf8 : "", -1);
            g_free(value_utf8);
            update_state_r(child, store, &iter);
            bugle_hash_set(&lookup, child->name, NULL);
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        }
        else
            valid = gtk_tree_store_remove(store, &iter);
    }

    /* Fill in missing items */
    valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &iter, parent);
    for (i = bugle_list_head(&root->children); i; i = bugle_list_next(i))
    {
        child = (const gldb_state *) bugle_list_data(i);

        if (!child->name) continue;
        if (bugle_hash_get(&lookup, child->name))
        {
            gchar *value;
            gchar *value_utf8;

            value = child->value ? child->value : "";
            value_utf8 = g_convert(value, -1, "UTF8", "ASCII", NULL, NULL, NULL);
            gtk_tree_store_insert_before(store, &iter2, parent,
                                         valid ? &iter : NULL);
            gtk_tree_store_set(store, &iter2,
                               COLUMN_STATE_NAME, child->name,
                               COLUMN_STATE_VALUE, value_utf8 ? value_utf8 : "",
                               -1);
            g_free(value_utf8);
            update_state_r(child, store, &iter2);
        }
        else if (valid)
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }

    bugle_hash_clear(&lookup);
}

static gboolean state_expose(GtkWidget *widget, GdkEventExpose *event,
                             gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    if (!context->state.dirty) return FALSE;
    context->state.dirty = FALSE;

    update_state_r(gldb_state_update(), context->state.state_store, NULL);
    return FALSE;
}

static const gldb_state *state_find_child_numeric(const gldb_state *parent,
                                                  GLint name)
{
    const gldb_state *child;
    bugle_list_node *i;

    /* FIXME: build indices on the state */
    for (i = bugle_list_head(&parent->children); i; i = bugle_list_next(i))
    {
        child = (const gldb_state *) bugle_list_data(i);
        if (child->numeric_name == name) return child;
    }
    return NULL;
}

static const gldb_state *state_find_child_enum(const gldb_state *parent,
                                               GLenum name)
{
    const gldb_state *child;
    bugle_list_node *i;

    /* FIXME: build indices on the state */
    for (i = bugle_list_head(&parent->children); i; i = bugle_list_next(i))
    {
        child = (const gldb_state *) bugle_list_data(i);
        if (child->enum_name == name) return child;
    }
    return NULL;
}

#if HAVE_GTKGLEXT
static void texture_draw_realize(GtkWidget *widget, gpointer user_data)
{
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    glcontext = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) return;

    glViewport(0, 0, widget->allocation.width, widget->allocation.height);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, context->texture.max_viewport_dims);

    gdk_gl_drawable_gl_end(gldrawable);
}

static gboolean texture_draw_configure(GtkWidget *widget,
                                       GdkEventConfigure *event,
                                       gpointer user_data)
{
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;

    glcontext = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) return FALSE;

    glViewport(0, 0, widget->allocation.width, widget->allocation.height);

    gdk_gl_drawable_gl_end(gldrawable);
    return TRUE;
}

static gboolean texture_draw_expose(GtkWidget *widget,
                                    GdkEventExpose *event,
                                    gpointer user_data)
{
    GldbWindow *context;
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;
    GLint width, height;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint level;
    guint mag, min;
    static const GLint vertices[8] =
    {
        -1, -1,
        1, -1,
        1, 1,
        -1, 1
    };
    static const GLint texcoords[8] =
    {
        0, 0,
        1, 0,
        1, 1,
        0, 1
    };

    static const GLint cube_vertices[24] =
    {
        -1, -1, -1,
        -1, -1, +1,
        -1, +1, -1,
        -1, +1, +1,
        +1, -1, -1,
        +1, -1, +1,
        +1, +1, -1,
        +1, +1, +1
    };
    static const GLubyte cube_indices[24] =
    {
        0, 1, 3, 2,
        0, 4, 5, 1,
        0, 2, 6, 4,
        7, 5, 4, 6,
        7, 6, 2, 3,
        7, 3, 1, 5
    };

#define GLDB_ISQRT2 0.70710678118654752440
#define GLDB_ISQRT3 0.57735026918962576451
#define GLDB_ISQRT6 0.40824829046386301636
    /* Rotation for looking at the (1, 1, 1) corner of the cube while
     * maintaining an up direction.
     */
    static const GLdouble cube_matrix1[16] =
    {
         GLDB_ISQRT2, -GLDB_ISQRT6,        GLDB_ISQRT3,  0.0,
         0.0,          2.0 * GLDB_ISQRT6,  GLDB_ISQRT3,  0.0,
        -GLDB_ISQRT2, -GLDB_ISQRT6,        GLDB_ISQRT3,  0.0,
         0.0,          0.0,                0.0,          1.0
    };
    /* Similar, but looking at (-1, -1, -1) */
    static const GLdouble cube_matrix2[16] =
    {
        -GLDB_ISQRT2, -GLDB_ISQRT6,       -GLDB_ISQRT3,  0.0,
         0.0,          2.0 * GLDB_ISQRT6, -GLDB_ISQRT3,  0.0,
         GLDB_ISQRT2, -GLDB_ISQRT6,       -GLDB_ISQRT3,  0.0,
         0.0,          0.0,                0.0,          1.0
    };

    context = (GldbWindow *) user_data;

    glcontext = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) return FALSE;
    glClear(GL_COLOR_BUFFER_BIT);

    if (context->texture.display_target == GL_NONE) goto no_texture;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.level));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.level), &iter))
        goto no_texture;
    gtk_tree_model_get(model, &iter, COLUMN_TEXTURE_LEVEL_VALUE, &level, -1);
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.mag_filter));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.mag_filter), &iter))
        goto no_texture;
    gtk_tree_model_get(model, &iter, COLUMN_TEXTURE_FILTER_VALUE, &mag, -1);
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.min_filter));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.min_filter), &iter))
        goto no_texture;
    gtk_tree_model_get(model, &iter, COLUMN_TEXTURE_FILTER_VALUE, &min, -1);

    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* FIXME: 3D textures */
    /* FIXME: non-RGBA textures */
    glEnable(context->texture.display_target);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

#ifdef GL_NV_texture_rectangle
    if (context->texture.display_target != GL_TEXTURE_RECTANGLE_NV)
#endif
    {
        if (level == -1)
        {
            glTexParameteri(context->texture.display_target, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(context->texture.display_target, GL_TEXTURE_MAX_LEVEL, 1000);
        }
        else
        {
            glTexParameteri(context->texture.display_target, GL_TEXTURE_BASE_LEVEL, level);
            glTexParameteri(context->texture.display_target, GL_TEXTURE_MAX_LEVEL, level);
        }
    }

    switch (context->texture.display_target)
    {
#ifdef GL_NV_texture_rectangle
    case GL_TEXTURE_RECTANGLE_NV:
        glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_NV, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_RECTANGLE_NV, 0, GL_TEXTURE_HEIGHT, &height);
        glMatrixMode(GL_TEXTURE);
        glScalef(width, height, 1.0);
        glMatrixMode(GL_MODELVIEW);
        /* fall through */
#endif
    case GL_TEXTURE_1D:
    case GL_TEXTURE_2D:
        glTexParameteri(context->texture.display_target, GL_TEXTURE_MAG_FILTER, mag);
        glTexParameteri(context->texture.display_target, GL_TEXTURE_MIN_FILTER, min);
        glVertexPointer(2, GL_INT, 0, vertices);
        glTexCoordPointer(2, GL_INT, 0, texcoords);
        glDrawArrays(GL_QUADS, 0, 4);
        break;
#ifdef GL_ARB_texture_cube_map
    case GL_TEXTURE_CUBE_MAP_ARB:
        /* Force all Z coordinates to 0 to avoid face clipping, and
         * scale down to fit in left half of window */
        glTranslatef(-0.5f, 0.0f, 0.0f);
        glScalef(0.25f, 0.5f, 0.0f);
        glMultMatrixd(cube_matrix1);

        glEnable(GL_CULL_FACE);
        glTexParameteri(context->texture.display_target, GL_TEXTURE_MAG_FILTER, mag);
        glTexParameteri(context->texture.display_target, GL_TEXTURE_MIN_FILTER, min);
        glVertexPointer(3, GL_INT, 0, cube_vertices);
        glTexCoordPointer(3, GL_INT, 0, cube_vertices);
        glDrawElements(GL_QUADS, 24, GL_UNSIGNED_BYTE, cube_indices);

        /* Draw second view */
        glLoadIdentity();
        glTranslatef(0.5f, 0.0f, 0.0f);
        glScalef(0.25f, 0.5, 0.5f);
        glMultMatrixd(cube_matrix2);
        glDrawElements(GL_QUADS, 24, GL_UNSIGNED_BYTE, cube_indices);
        break;
#endif
    default:
        break;
    }

    glPopClientAttrib();
    glPopAttrib();

    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);
    return TRUE;

no_texture:
    glClear(GL_COLOR_BUFFER_BIT);
    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);
    return TRUE;
}

static void resize_texture_draw(GldbWindow *context)
{
    GtkWidget *aspect, *alignment, *draw;
    GtkTreeModel *zoom_model;
    GtkTreeIter iter;
    gdouble zoom;
    int width, height;

    draw = context->texture.draw;
    aspect = gtk_widget_get_parent(draw);
    alignment = gtk_widget_get_parent(aspect);
    width = context->texture.width;
    height = context->texture.height;

    zoom_model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.zoom));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.zoom),
                                      &iter))
    {
        gtk_tree_model_get(zoom_model, &iter,
                           COLUMN_TEXTURE_ZOOM_VALUE, &zoom, -1);
        if (zoom < 0.0)
        {
            /* Fit */
            gtk_widget_set_size_request(draw, 1, 1);
            gtk_alignment_set(GTK_ALIGNMENT(alignment), 0.5f, 0.5f, 1.0f, 1.0f);
            gtk_aspect_frame_set(GTK_ASPECT_FRAME(aspect),
                                 0.5f, 0.5f, (gfloat) width / height, FALSE);
        }
        else
        {
            /* fixed */
            width = (int) ceil(width * zoom);
            height = (int) ceil(height * zoom);
            gtk_widget_set_size_request(draw, width, height);
            gtk_alignment_set(GTK_ALIGNMENT(alignment), 0.5f, 0.5f, 0.0f, 0.0f);
            gtk_aspect_frame_set(GTK_ASPECT_FRAME(aspect),
                                 0.5f, 0.5f, 1.0f, TRUE);
        }
    }
}

static gboolean response_callback_texture(GldbWindow *context,
                                          gldb_response *response,
                                          gpointer user_data)
{
    gldb_response_data_texture *r;
    texture_callback_data *data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean valid, more;
    gint sensitive;

    r = (gldb_response_data_texture *) response;
    data = (texture_callback_data *) user_data;

    if (data->flags & TEXTURE_CALLBACK_FLAG_FIRST)
    {
        GLuint id;

        /* We abuse the fact that we may assign our own IDs to textures,
         * and use the target token as the texture ID.
         */
        id = data->target;
        glDeleteTextures(1, &id);
        glBindTexture(data->target, id);

        context->texture.display_target = GL_NONE;
    }

    if (response->code != RESP_DATA || !r->length)
    {
        /* FIXME: tag the texture as invalid */
    }
    else
    {
        GdkGLContext *glcontext;
        GdkGLDrawable *gldrawable;

        if (data->flags & TEXTURE_CALLBACK_FLAG_FIRST)
        {
            context->texture.width = r->width;
            context->texture.height = r->height;
#ifdef GL_ARB_texture_cube_map
            /* Make space for two views of the cube */
            if (data->target == GL_TEXTURE_CUBE_MAP_ARB)
                context->texture.width *= 2;
#endif
        }
        glcontext = gtk_widget_get_gl_context(context->texture.draw);
        gldrawable = gtk_widget_get_gl_drawable(context->texture.draw);

        /* FIXME: check runtime support for all extensions */
        if (gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        {
            /* FIXME: multiple TexParameteri calls are overkill */
            switch (data->target)
            {
            case GL_TEXTURE_1D:
                glTexImage1D(data->target, data->level, GL_RGBA8, r->width, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, r->data);
                glTexParameteri(data->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                break;
            case GL_TEXTURE_2D:
#ifdef GL_NV_texture_rectangle
            case GL_TEXTURE_RECTANGLE_NV:
#endif
                glTexImage2D(data->target, data->level, GL_RGBA8, r->width, r->height, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, r->data);
                glTexParameteri(data->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(data->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                break;
#ifdef GL_ARB_texture_cube_map
            case GL_TEXTURE_CUBE_MAP_ARB:
                glTexImage2D(data->face, data->level, GL_RGBA8, r->width, r->height, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, r->data);
                glTexParameteri(data->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(data->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                break;
#endif
#ifdef GL_EXT_texture3D
            case GL_TEXTURE_3D_EXT:
                glTexImage3DEXT(data->face, data->level, GL_RGBA8, r->width, r->height, r->depth, 0,
                                GL_RGBA, GL_UNSIGNED_BYTE, r->data);
                glTexParameteri(data->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(data->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(data->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                break;
#endif
            default:
                g_error("Texture target is not supported.\n");
            }
            gdk_gl_drawable_gl_end(gldrawable);
        }
    }

    if (data->flags & TEXTURE_CALLBACK_FLAG_LAST)
    {
        context->texture.display_target = data->target;

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.zoom));
        more = gtk_tree_model_get_iter_first(model, &iter);
        while (more)
        {
            gdouble zoom;

            gtk_tree_model_get(model, &iter, COLUMN_TEXTURE_ZOOM_VALUE, &zoom, -1);
            if (zoom > 0.0)
            {
                valid = zoom * context->texture.width < context->texture.max_viewport_dims[0]
                    && zoom * context->texture.height < context->texture.max_viewport_dims[1]
                    && zoom * context->texture.width >= 1.0
                    && zoom * context->texture.height >= 1.0;
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COLUMN_TEXTURE_ZOOM_SENSITIVE, valid, -1);
            }
            more = gtk_tree_model_iter_next(model, &iter);
        }
        valid = FALSE;
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.zoom), &iter))
            gtk_tree_model_get(model, &iter, COLUMN_TEXTURE_ZOOM_SENSITIVE, &valid, -1);
        /* FIXME: pick nearest zoom level rather than "Fit" */
        if (!valid)
            gtk_combo_box_set_active(GTK_COMBO_BOX(context->texture.zoom), 0);
        else
            /* Updates the sensitivity of the zoom buttons if necessary */
            g_signal_emit_by_name(G_OBJECT(context->texture.zoom), "changed", context);

#ifdef GL_NV_texture_rectangle
        sensitive = (context->texture.display_target == GL_TEXTURE_RECTANGLE_NV)
            ? COLUMN_TEXTURE_FILTER_NON_MIP : COLUMN_TEXTURE_FILTER_TRUE;
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(context->texture.min_filter),
                                       context->texture.min_filter_cell,
                                       "text", COLUMN_TEXTURE_FILTER_TEXT,
                                       "sensitive", sensitive,
                                       NULL);
        valid = FALSE;
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.min_filter));
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.min_filter), &iter))
            gtk_tree_model_get(model, &iter, sensitive, &valid, -1);
        if (!valid)
            gtk_combo_box_set_active(GTK_COMBO_BOX(context->texture.min_filter), 0);
#endif

        resize_texture_draw(context);
        gtk_widget_queue_draw(context->texture.draw);
    }

    gldb_free_response(response);
    free(data);
    return TRUE;
}

static void update_texture_ids(GldbWindow *context);

static gboolean texture_expose(GtkWidget *widget, GdkEventExpose *event,
                               gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    if (!context->texture.dirty) return FALSE;
    context->texture.dirty = FALSE;

    /* Simply invoke a change event on the selector. This launches a
     * cascade of updates.
     */
    update_texture_ids(context);
    g_signal_emit_by_name(G_OBJECT(context->texture.id), "changed", NULL, NULL);
    return FALSE;
}
#endif /* HAVE_GTKGLEXT */

#ifdef GLDB_GUI_SHADER
static gboolean response_callback_shader(GldbWindow *context,
                                         gldb_response *response,
                                         gpointer user_data)
{
    gldb_response_data_shader *r;
    GtkTextBuffer *buffer;

    r = (gldb_response_data_shader *) response;
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(context->shader.source));
    if (response->code != RESP_DATA || !r->length)
        gtk_text_buffer_set_text(buffer, "", 0);
    else
    {
        gint lines, i;
        GtkTextIter iter, start, end;
        gchar lineno[64];

        gtk_text_buffer_set_text(buffer, r->data, r->length);
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        gtk_text_buffer_apply_tag_by_name(buffer, "default", &start, &end);
        lines = gtk_text_buffer_get_line_count(buffer);
        for (i = 0; i < lines; i++)
        {
            gtk_text_buffer_get_iter_at_line(buffer, &iter, i);
            g_snprintf(lineno, G_N_ELEMENTS(lineno), "%4d: ", (int) i + 1);
            gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, lineno, -1,
                                                    "default", "lineno", NULL);
        }
    }
    gldb_free_response(response);
    return TRUE;
}

static gboolean shader_expose(GtkWidget *widget, GdkEventExpose *event,
                              gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    if (!context->shader.dirty) return FALSE;
    context->shader.dirty = FALSE;

    /* Simply invoke a change event on the target. This launches a
     * cascade of updates.
     */
    g_signal_emit_by_name(G_OBJECT(context->shader.target), "changed", NULL, NULL);
    return FALSE;
}
#endif

static gboolean backtrace_expose(GtkWidget *widget, GdkEventExpose *event,
                                 gpointer user_data)
{
    gchar *argv[4];
    gint in, out;
    GError *error = NULL;
    GIOChannel *inf, *outf;
    gchar *line;
    gsize terminator;
    GldbWindow *context;
    const gchar *cmds = "set width 0\nset height 0\nbacktrace\nquit\n";

    context = (GldbWindow *) user_data;
    if (!context->backtrace.dirty) return FALSE;
    context->backtrace.dirty = FALSE;

    gtk_list_store_clear(context->backtrace.backtrace_store);
    argv[0] = g_strdup("gdb");
    argv[1] = g_strdup(gldb_get_program());
    bugle_asprintf(&argv[2], "%lu", (unsigned long) gldb_get_child_pid());
    argv[3] = NULL;
    if (g_spawn_async_with_pipes(NULL,    /* working directory */
                                 argv,
                                 NULL,    /* envp */
                                 G_SPAWN_SEARCH_PATH,
                                 NULL,    /* setup func */
                                 NULL,    /* user_data */
                                 NULL,    /* PID pointer */
                                 &in,
                                 &out,
                                 NULL,    /* stderr pointer */
                                 &error))
    {
        /* Note: in and out refer to the child's view, so we swap here */
        inf = g_io_channel_unix_new(out);
        outf = g_io_channel_unix_new(in);
        g_io_channel_set_close_on_unref(outf, TRUE);
        g_io_channel_set_close_on_unref(inf, TRUE);
        g_io_channel_set_encoding(inf, NULL, NULL);
        g_io_channel_set_encoding(outf, NULL, NULL);
        g_io_channel_write_chars(outf, cmds, strlen(cmds), NULL, NULL);
        g_io_channel_flush(outf, NULL);

        while (g_io_channel_read_line(inf, &line, NULL, &terminator, &error) == G_IO_STATUS_NORMAL
               && line != NULL)
            if (line[0] == '#')
            {
                GtkTreeIter iter;

                line[terminator] = '\0';
                gtk_list_store_append(context->backtrace.backtrace_store, &iter);
                gtk_list_store_set(context->backtrace.backtrace_store, &iter,
                                   0, line, -1);
                g_free(line);
            }
        g_io_channel_unref(inf);
        g_io_channel_unref(outf);
    }
    g_free(argv[0]);
    g_free(argv[1]);
    free(argv[2]);

    return FALSE;
}

static void update_status_bar(GldbWindow *context, const gchar *text)
{
    gtk_statusbar_pop(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id);
    gtk_statusbar_push(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id, text);
}

static void stopped(GldbWindow *context, const gchar *text)
{
    context->state.dirty = true;
    gtk_widget_queue_draw(context->state.page);
#if HAVE_GTKGLEXT
    context->texture.dirty = true;
    gtk_widget_queue_draw(context->texture.page);
#endif
#ifdef GLDB_GUI_SHADER
    context->shader.dirty = true;
    gtk_widget_queue_draw(context->shader.page);
#endif
    context->backtrace.dirty = true;
    gtk_widget_queue_draw(context->backtrace.page);
    gtk_action_group_set_sensitive(context->running_actions, FALSE);
    gtk_action_group_set_sensitive(context->stopped_actions, TRUE);

    update_status_bar(context, text);
}

static void child_exit_callback(GPid pid, gint status, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    if (context->channel)
    {
        g_io_channel_unref(context->channel);
        context->channel = NULL;
        g_source_remove(context->channel_watch);
        context->channel_watch = 0;
    }

    gldb_notify_child_dead();
    update_status_bar(context, _("Not running"));
    gtk_action_group_set_sensitive(context->running_actions, FALSE);
    gtk_action_group_set_sensitive(context->stopped_actions, FALSE);
    gtk_action_group_set_sensitive(context->dead_actions, TRUE);
    gtk_list_store_clear(context->backtrace.backtrace_store);
}

static gboolean response_callback(GIOChannel *channel, GIOCondition condition,
                                  gpointer user_data)
{
    GldbWindow *context;
    gldb_response *r;
    char *msg;
    gboolean done = false;
    bugle_list_node *n;
    response_handler *h;

    context = (GldbWindow *) user_data;
    r = gldb_get_response();

    n = bugle_list_head(&context->response_handlers);
    if (n) h = (response_handler *) bugle_list_data(n);
    while (n && h->id < r->id)
    {
        bugle_list_erase(&context->response_handlers, n);
        n = bugle_list_head(&context->response_handlers);
        if (n) h = (response_handler *) bugle_list_data(n);
    }
    while (n && h->id == r->id)
    {
        done = (*h->callback)(context, r, h->user_data);
        bugle_list_erase(&context->response_handlers, n);
        if (done) return TRUE;
        n = bugle_list_head(&context->response_handlers);
        if (n) h = (response_handler *) bugle_list_data(n);
    }

    switch (r->code)
    {
    case RESP_STOP:
        bugle_asprintf(&msg, _("Stopped in %s"), ((gldb_response_stop *) r)->call);
        stopped(context, msg);
        free(msg);
        break;
    case RESP_BREAK:
        bugle_asprintf(&msg, _("Break on %s"), ((gldb_response_break *) r)->call);
        stopped(context, msg);
        free(msg);
        break;
    case RESP_BREAK_ERROR:
        bugle_asprintf(&msg, _("%s in %s"),
                       ((gldb_response_break_error *) r)->error,
                       ((gldb_response_break_error *) r)->call);
        stopped(context, msg);
        free(msg);
        break;
    case RESP_ERROR:
        /* FIXME: display the error */
        break;
    case RESP_RUNNING:
        update_status_bar(context, _("Running"));
        gtk_action_group_set_sensitive(context->running_actions, TRUE);
        gtk_action_group_set_sensitive(context->stopped_actions, FALSE);
        gtk_action_group_set_sensitive(context->dead_actions, FALSE);
        break;
    }

    return TRUE;
}

static void quit_action(GtkAction *action, gpointer user_data)
{
    gtk_widget_destroy(((GldbWindow *) user_data)->window);
}

static void child_init(void)
{
}

static void run_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    if (gldb_get_status() != GLDB_STATUS_DEAD)
    {
        /* FIXME */
    }
    else
    {
        /* GIOChannel on UNIX is sufficiently light that we may wrap
         * the input pipe in it without forcing us to do everything through
         * it.
         */
        gldb_run(seq++, child_init);
        context->channel = g_io_channel_unix_new(gldb_get_in_pipe());
        context->channel_watch = g_io_add_watch(context->channel, G_IO_IN | G_IO_ERR,
                                                response_callback, context);
        g_child_watch_add(gldb_get_child_pid(), child_exit_callback, context);
        update_status_bar(context, _("Started"));
    }
}

static void stop_action(GtkAction *action, gpointer user_data)
{
    if (gldb_get_status() == GLDB_STATUS_RUNNING)
        gldb_send_async(seq++);
}

static void continue_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    gtk_action_group_set_sensitive(context->stopped_actions, FALSE);
    gtk_action_group_set_sensitive(context->running_actions, TRUE);
    gldb_send_continue(seq++);
    update_status_bar((GldbWindow *) user_data, _("Running"));
}

static void kill_action(GtkAction *action, gpointer user_data)
{
    gldb_send_quit(seq++);
}

static void chain_action(GtkAction *action, gpointer user_data)
{
    GtkWidget *dialog, *entry;
    GldbWindow *context;
    gint result;

    context = (GldbWindow *) user_data;
    dialog = gtk_dialog_new_with_buttons(_("Filter chain"),
                                         GTK_WINDOW(context->window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    entry = gtk_entry_new();
    if (gldb_get_chain())
        gtk_entry_set_text(GTK_ENTRY(entry), gldb_get_chain());
    gtk_widget_show(entry);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), entry, TRUE, FALSE, 0);

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT)
        gldb_set_chain(gtk_entry_get_text(GTK_ENTRY(entry)));
    gtk_widget_destroy(dialog);
}

static void breakpoints_add_action(GtkButton *button, gpointer user_data)
{
    GldbBreakpointsDialog *context;
    GtkWidget *dialog, *entry;
    GtkEntryCompletion *completion;
    gint result;

    context = (GldbBreakpointsDialog *) user_data;

    dialog = gtk_dialog_new_with_buttons(_("Add breakpoint"),
                                         GTK_WINDOW(context->dialog),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    entry = gtk_entry_new();
    completion = gtk_entry_completion_new();
    gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(function_names));
    gtk_entry_completion_set_minimum_key_length(completion, 4);
    gtk_entry_completion_set_text_column(completion, 0);
    gtk_entry_set_completion(GTK_ENTRY(entry), completion);
    gtk_widget_show(entry);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), entry, TRUE, FALSE, 0);

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT)
    {
        GtkTreeIter iter;

        gtk_list_store_append(context->parent->breakpoints_store, &iter);
        gtk_list_store_set(context->parent->breakpoints_store, &iter,
                           COLUMN_BREAKPOINTS_FUNCTION, gtk_entry_get_text(GTK_ENTRY(entry)),
                           -1);
        gldb_set_break(seq++, gtk_entry_get_text(GTK_ENTRY(entry)), true);
    }
    gtk_widget_destroy(dialog);
}

static void breakpoints_remove_action(GtkButton *button, gpointer user_data)
{
    GldbBreakpointsDialog *context;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *model;
    const gchar *func;

    context = (GldbBreakpointsDialog *) user_data;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(context->list));
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_tree_model_get(model, &iter, 0, &func, -1);
        gldb_set_break(seq++, func, false);
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    }
}

static void breakpoints_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *pcontext;
    GldbBreakpointsDialog context;
    GtkWidget *hbox, *bbox, *btn, *toggle;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    gint result;

    pcontext = (GldbWindow *) user_data;
    context.parent = pcontext;
    context.dialog = gtk_dialog_new_with_buttons(_("Breakpoints"),
                                                 GTK_WINDOW(pcontext->window),
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                                 NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(context.dialog), GTK_RESPONSE_ACCEPT);

    hbox = gtk_hbox_new(FALSE, 0);

    context.list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pcontext->breakpoints_store));
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Function"),
                                                      cell,
                                                      "text", COLUMN_BREAKPOINTS_FUNCTION,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(context.list), column);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(context.list), FALSE);
    gtk_widget_set_size_request(context.list, 300, 200);
    gtk_box_pack_start(GTK_BOX(hbox), context.list, TRUE, TRUE, 0);
    gtk_widget_show(context.list);

    bbox = gtk_vbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
    btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(breakpoints_add_action), &context);
    gtk_widget_show(btn);
    gtk_box_pack_start(GTK_BOX(bbox), btn, TRUE, FALSE, 0);
    btn = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(breakpoints_remove_action), &context);
    gtk_widget_show(btn);
    gtk_box_pack_start(GTK_BOX(bbox), btn, TRUE, FALSE, 0);
    gtk_widget_show(bbox);
    gtk_box_pack_start(GTK_BOX(hbox), bbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox), hbox, TRUE, TRUE, 0);

    toggle = gtk_check_button_new_with_mnemonic(_("Break on _errors"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), gldb_get_break_error());
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox), toggle, FALSE, FALSE, 0);
    gtk_widget_show(toggle);

    gtk_dialog_set_default_response(GTK_DIALOG(context.dialog), GTK_RESPONSE_ACCEPT);

    result = gtk_dialog_run(GTK_DIALOG(context.dialog));
    if (result == GTK_RESPONSE_ACCEPT)
        gldb_set_break_error(seq++, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));
    gtk_widget_destroy(context.dialog);
}

static void build_state_page(GldbWindow *context)
{
    GtkWidget *scrolled, *tree_view, *label;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    gint page;

    context->state.state_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(context->state.state_store));
    cell = gtk_cell_renderer_text_new();
    g_object_set(cell, "yalign", 0.0, NULL);
    column = gtk_tree_view_column_new_with_attributes(_("Name"),
                                                      cell,
                                                      "text", COLUMN_STATE_NAME,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_STATE_NAME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Value"),
                                                      cell,
                                                      "text", COLUMN_STATE_VALUE,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree_view), COLUMN_STATE_NAME);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_view), TRUE);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), tree_view);
    gtk_widget_show(scrolled);
    g_object_unref(G_OBJECT(context->state.state_store)); /* So that it dies with the view */

    label = gtk_label_new(_("State"));
    page = gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook), scrolled, label);
    context->state.dirty = false;
    context->state.page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(context->notebook), page);
    g_signal_connect(G_OBJECT(context->state.page), "expose-event",
                     G_CALLBACK(state_expose), context);
}

#if HAVE_GTKGLEXT
static void update_texture_ids(GldbWindow *context)
{
    const gldb_state *root, *s, *t, *l, *f;
    GtkTreeModel *model;
    GtkTreeIter iter, old_iter;
    guint old_id, old_target;
    gboolean have_old = FALSE, have_old_iter = FALSE;
    bugle_list_node *nt, *nl;
    gchar *name;
    guint levels;

    const GLenum targets[] = {
        GL_TEXTURE_1D,
        GL_TEXTURE_2D,
#ifdef GL_EXT_texture3D
        GL_TEXTURE_3D_EXT,
#endif
#ifdef GL_ARB_texture_cube_map
        GL_TEXTURE_CUBE_MAP_ARB,
#endif
#ifdef GL_NV_texture_rectangle
        GL_TEXTURE_RECTANGLE_NV
#endif
    };
    const gchar * const target_names[] = {
        "1D",
        "2D",
#ifdef GL_EXT_texture3D
        "3D",
#endif
#ifdef GL_ARB_texture_cube_map
        "cube",
#endif
#ifdef GL_NV_texture_rectangle
        "rect"
#endif
    };
    guint trg;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.id));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.id), &iter))
    {
        have_old = TRUE;
        gtk_tree_model_get(model, &iter,
                           COLUMN_TEXTURE_ID_ID, &old_id,
                           COLUMN_TEXTURE_ID_TARGET, &old_target,
                           -1);
    }
    gtk_list_store_clear(GTK_LIST_STORE(model));

    root = gldb_state_update(); /* FIXME: manage state tree ourselves */

    for (trg = 0; trg < G_N_ELEMENTS(targets); trg++)
    {
        s = state_find_child_enum(root, targets[trg]);
        if (!s) continue;
        for (nt = bugle_list_head(&s->children); nt != NULL; nt = bugle_list_next(nt))
        {
            t = (const gldb_state *) bugle_list_data(nt);
            if (t->enum_name == 0)
            {
                /* Count the levels */
                f = t;
#ifdef GL_ARB_texture_cube_map
                if (targets[trg] == GL_TEXTURE_CUBE_MAP_ARB)
                    f = state_find_child_enum(t, GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB);
#endif
                levels = 0;
                for (nl = bugle_list_head(&f->children); nl != NULL; nl = bugle_list_next(nl))
                {
                    l = (const gldb_state *) bugle_list_data(nl);
                    if (l->enum_name == 0)
                        levels = MAX(levels, (guint) l->numeric_name + 1);
                }
                if (!levels) continue;
                bugle_asprintf(&name, "%u (%s)", (unsigned int) t->numeric_name, target_names[trg]);
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COLUMN_TEXTURE_ID_ID, (guint) t->numeric_name,
                                   COLUMN_TEXTURE_ID_TARGET, (guint) targets[trg],
                                   COLUMN_TEXTURE_ID_LEVELS, levels,
                                   COLUMN_TEXTURE_ID_TEXT, name,
                                   -1);
                free(name);
                if (have_old && t->numeric_name == (GLint) old_id && targets[trg] == (GLenum) old_target)
                {
                    old_iter = iter;
                    have_old_iter = TRUE;
                }
            }
        }
    }
    if (have_old_iter)
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(context->texture.id),
                                      &old_iter);
    else
        gtk_combo_box_set_active(GTK_COMBO_BOX(context->texture.id), 0);
}

static void texture_id_changed(GtkComboBox *id_box, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint target, id, levels;
    guint i, l;
    GldbWindow *context;
    texture_callback_data *data;
    gint old;

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
    {
        context = (GldbWindow *) user_data;

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.id));
        if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.id), &iter)) return;
        gtk_tree_model_get(model, &iter,
                           COLUMN_TEXTURE_ID_ID, &id,
                           COLUMN_TEXTURE_ID_TARGET, &target,
                           COLUMN_TEXTURE_ID_LEVELS, &levels,
                           -1);

        for (l = 0; l < levels; l++)
        {
#ifdef GL_ARB_texture_cube_map
            if (target == GL_TEXTURE_CUBE_MAP_ARB)
            {
                for (i = 0; i < 6; i++)
                {
                    data = (texture_callback_data *) bugle_malloc(sizeof(texture_callback_data));
                    data->target = target;
                    data->face = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + i;
                    data->level = l;
                    data->flags = 0;
                    if (l == 0 && i == 0) data->flags |= TEXTURE_CALLBACK_FLAG_FIRST;
                    if (l == levels - 1 && i == 5) data->flags |= TEXTURE_CALLBACK_FLAG_LAST;
                    set_response_handler(context, seq, response_callback_texture, data);
                    gldb_send_data_texture(seq++, id, target, data->face, l,
                                           GL_RGBA, GL_UNSIGNED_BYTE);
                }
            }
            else
#endif
            {
                data = (texture_callback_data *) bugle_malloc(sizeof(texture_callback_data));
                data->target = target;
                data->face = target;
                data->level = l;
                data->flags = 0;
                if (l == 0) data->flags |= TEXTURE_CALLBACK_FLAG_FIRST;
                if (l == levels - 1) data->flags |= TEXTURE_CALLBACK_FLAG_LAST;
                set_response_handler(context, seq, response_callback_texture, data);
                gldb_send_data_texture(seq++, id, target, target, l,
                                       GL_RGBA, GL_UNSIGNED_BYTE);
            }
        }

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.level));
        old = gtk_combo_box_get_active(GTK_COMBO_BOX(context->texture.level));
        gtk_list_store_clear(GTK_LIST_STORE(model));
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COLUMN_TEXTURE_LEVEL_VALUE, -1,
                           COLUMN_TEXTURE_LEVEL_TEXT, _("Auto"),
                           -1);
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COLUMN_TEXTURE_LEVEL_VALUE, -2, /* -2 is magic separator value */
                           COLUMN_TEXTURE_LEVEL_TEXT, "Separator",
                           -1);
        for (i = 0; i < levels; i++)
        {
            char *text;

            bugle_asprintf(&text, "%d", i);
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COLUMN_TEXTURE_LEVEL_VALUE, (gint) i,
                               COLUMN_TEXTURE_LEVEL_TEXT, text,
                               -1);
            free(text);
        }
        if (old <= (gint) levels) gtk_combo_box_set_active(GTK_COMBO_BOX(context->texture.level), old);
        else gtk_combo_box_set_active(GTK_COMBO_BOX(context->texture.level), levels);
    }
}

static void texture_level_changed(GtkWidget *widget, gpointer user_data)
{
    GldbWindow *context;

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
    {
        context = (GldbWindow *) user_data;
        gtk_widget_queue_draw(context->texture.draw);
    }
}

static void texture_filter_changed(GtkWidget *widget, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    gtk_widget_queue_draw(context->texture.draw);
}

static void texture_zoom_changed(GtkWidget *zoom, gpointer user_data)
{
    GldbWindow *context;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter, next;
    gboolean sensitive_in = FALSE;
    gboolean sensitive_out = FALSE;
    gboolean sensitive_100 = FALSE;
    gboolean sensitive;
    gboolean more;
    gdouble value;

    context = (GldbWindow *) user_data;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(zoom));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(zoom), &iter))
    {
        next = iter;
        if (gtk_tree_model_iter_next(model, &next))
            gtk_tree_model_get(model, &next, COLUMN_TEXTURE_ZOOM_SENSITIVE, &sensitive_in, -1);
        path = gtk_tree_model_get_path(model, &iter);
        if (gtk_tree_path_prev(path))
        {
            gtk_tree_model_get_iter(model, &iter, path);
            gtk_tree_model_get(model, &iter, COLUMN_TEXTURE_ZOOM_SENSITIVE, &sensitive_out, -1);
        }
        gtk_tree_path_free(path);
    }

    more = gtk_tree_model_get_iter_first(model, &iter);
    while (more)
    {
        gtk_tree_model_get(model, &iter,
                           COLUMN_TEXTURE_ZOOM_VALUE, &value,
                           COLUMN_TEXTURE_ZOOM_SENSITIVE, &sensitive,
                           -1);
        if (value == 1.0)
        {
            sensitive_100 = sensitive;
            break;
        }
        more = gtk_tree_model_iter_next(model, &iter);
    }
    gtk_widget_set_sensitive(GTK_WIDGET(context->texture.zoom_in), sensitive_in);
    gtk_widget_set_sensitive(GTK_WIDGET(context->texture.zoom_out), sensitive_out);
    gtk_widget_set_sensitive(GTK_WIDGET(context->texture.zoom_100), sensitive_100);

    resize_texture_draw(context);
}

static void free_pixbuf_data(guchar *pixels, gpointer user_data)
{
    free(pixels);
}

static void texture_copy_clicked(GtkWidget *button, gpointer user_data)
{
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;
    GLint width = 1, height = 1, r1, r2;
    GLubyte *pixels, *row;
    GdkPixbuf *pixbuf = NULL;
    GtkClipboard *clipboard;
    GldbWindow *context;
    GLenum target;
    gint level;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkWidget *dialog;

    context = (GldbWindow *) user_data;
    if (context->texture.display_target == GL_NONE) return;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.level));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.level), &iter))
        return;
    gtk_tree_model_get(model, &iter, COLUMN_TEXTURE_LEVEL_VALUE, &level, -1);
    if (level < 0) level = 0;

    glcontext = gtk_widget_get_gl_context(context->texture.draw);
    gldrawable = gtk_widget_get_gl_drawable(context->texture.draw);
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) return;

    /* FIXME: handle other targets */
    /* FIXME: disable button when appropriate */
    target = context->texture.display_target;
    switch (target)
    {
    case GL_TEXTURE_1D:
    case GL_TEXTURE_2D:
#ifdef GL_NV_texture_rectangle
    case GL_TEXTURE_RECTANGLE_NV:
#endif
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
        if (target != GL_TEXTURE_1D)
            glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
        pixels = (GLubyte *) bugle_malloc(width * height * 4);
        row = (GLubyte *) bugle_malloc(width * 4); /* temp storage for swaps */
        glGetTexImage(target, level, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        /* Flip vertically */
        for (r1 = 0, r2 = height - 1; r1 < r2; r1++, r2--)
        {
            memcpy(row, pixels + r1 * width * 4, width * 4);
            memcpy(pixels + r1 * width * 4, pixels + r2 * width * 4, width * 4);
            memcpy(pixels + r2 * width * 4, row, width * 4);
        }
        free(row);
        pixbuf = gdk_pixbuf_new_from_data(pixels, GDK_COLORSPACE_RGB, TRUE, 8,
                                          width, height, width * 4,
                                          free_pixbuf_data, NULL);
        glPopClientAttrib();
        break;
    default:
        dialog = gtk_message_dialog_new(GTK_WINDOW(context->window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        "Copying is not currently supported for this target.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        break;
    }
    gdk_gl_drawable_gl_end(gldrawable);

    if (!pixbuf) return;

    clipboard = gtk_clipboard_get_for_display(gtk_widget_get_display(button),
                                              GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_image(clipboard, pixbuf);
    g_object_unref(pixbuf);
}

static void texture_zoom_in_clicked(GtkToolButton *toolbutton,
                                    gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GldbWindow *context;
    gboolean sensitive;

    context = (GldbWindow *) user_data;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.zoom));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.zoom), &iter)
        && gtk_tree_model_iter_next(model, &iter))
    {
        gtk_tree_model_get(model, &iter, COLUMN_TEXTURE_ZOOM_SENSITIVE, &sensitive, -1);
        if (sensitive)
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(context->texture.zoom), &iter);
    }
}

static void texture_zoom_out_clicked(GtkToolButton *toolbutton,
                                     gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreePath *path;
    GldbWindow *context;
    gboolean sensitive;

    context = (GldbWindow *) user_data;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.zoom));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->texture.zoom), &iter))
    {
        /* There is no gtk_tree_model_iter_prev, so we have to go via tree paths. */
        path = gtk_tree_model_get_path(model, &iter);
        if (gtk_tree_path_prev(path))
        {
            gtk_tree_model_get_iter(model, &iter, path);
            gtk_tree_model_get(model, &iter, COLUMN_TEXTURE_ZOOM_SENSITIVE, &sensitive, -1);
            if (sensitive)
                gtk_combo_box_set_active_iter(GTK_COMBO_BOX(context->texture.zoom), &iter);
        }
        gtk_tree_path_free(path);
    }
}

static void texture_zoom_100_clicked(GtkToolButton *toolbutton,
                                     gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GldbWindow *context;
    gdouble zoom;
    gboolean sensitive, more;

    context = (GldbWindow *) user_data;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.zoom));
    more = gtk_tree_model_get_iter_first(model, &iter);
    while (more)
    {
        gtk_tree_model_get(model, &iter,
                           COLUMN_TEXTURE_ZOOM_VALUE, &zoom,
                           COLUMN_TEXTURE_ZOOM_SENSITIVE, &sensitive,
                           -1);
        if (zoom == 1.0)
        {
            if (sensitive)
                gtk_combo_box_set_active_iter(GTK_COMBO_BOX(context->texture.zoom), &iter);
            return;
        }

        more = gtk_tree_model_iter_next(model, &iter);
    }
}

static void texture_zoom_fit_clicked(GtkToolButton *toolbutton,
                                     gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    gtk_combo_box_set_active(GTK_COMBO_BOX(context->texture.zoom), 0);
}

static GtkWidget *build_texture_page_id(GldbWindow *context)
{
    GtkListStore *store;
    GtkWidget *id;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(4, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
                                         COLUMN_TEXTURE_ID_ID,
                                         GTK_SORT_ASCENDING);
    id = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_widget_set_size_request(id, 80, -1);

    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(id), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(id), cell,
                                   "text", COLUMN_TEXTURE_ID_TEXT, NULL);
    g_signal_connect(G_OBJECT(id), "changed",
                     G_CALLBACK(texture_id_changed), context);
    context->texture.id = id;
    g_object_unref(G_OBJECT(store));
    return id;
}

static gboolean texture_level_row_separator(GtkTreeModel *model,
                                            GtkTreeIter *iter,
                                            gpointer user_data)
{
    gint value;

    gtk_tree_model_get(model, iter, COLUMN_TEXTURE_LEVEL_VALUE, &value, -1);
    return value == -2;
}

static GtkWidget *build_texture_page_level(GldbWindow *context)
{
    GtkListStore *store;
    GtkTreeIter iter;
    GtkWidget *level;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_TEXTURE_LEVEL_VALUE, -1,
                       COLUMN_TEXTURE_LEVEL_TEXT, _("Auto"),
                       -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_TEXTURE_LEVEL_VALUE, -2, /* -2 is magic separator value */
                       COLUMN_TEXTURE_LEVEL_TEXT, "Separator",
                       -1);

    context->texture.level = level = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(level), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(level), cell,
                                   "text", COLUMN_TEXTURE_LEVEL_TEXT, NULL);
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(level),
                                         texture_level_row_separator,
                                         NULL, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(level), 0);
    g_signal_connect(G_OBJECT(level), "changed",
                     G_CALLBACK(texture_level_changed), context);
    g_object_unref(G_OBJECT(store));
    return level;
}

static gboolean texture_zoom_row_separator(GtkTreeModel *model,
                                           GtkTreeIter *iter,
                                           gpointer data)
{
    gdouble value;

    gtk_tree_model_get(model, iter, COLUMN_TEXTURE_ZOOM_VALUE, &value, -1);
    return value == -2.0;
}

static GtkWidget *build_texture_page_zoom(GldbWindow *context)
{
    GtkListStore *store;
    GtkTreeIter iter;
    GtkWidget *zoom;
    GtkCellRenderer *cell;
    int i;

    store = gtk_list_store_new(3, G_TYPE_DOUBLE, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_TEXTURE_ZOOM_VALUE, -1.0,
                       COLUMN_TEXTURE_ZOOM_TEXT, _("Fit"),
                       COLUMN_TEXTURE_ZOOM_SENSITIVE, TRUE,
                       -1);

    /* Note: separator must be non-sensitive, because the zoom-out button
     * examines its sensitivity to see if it can zoom out further.
     */
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_TEXTURE_ZOOM_VALUE, -2.0, /* -2.0 is magic separator value - see above function */
                       COLUMN_TEXTURE_ZOOM_TEXT, "Separator",
                       COLUMN_TEXTURE_ZOOM_SENSITIVE, FALSE,
                       -1);

    for (i = 5; i >= 0; i--)
    {
        gchar *caption;

        bugle_asprintf(&caption, "1:%d", (1 << i));
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_TEXTURE_ZOOM_VALUE, (gdouble) 1.0 / (1 << i),
                           COLUMN_TEXTURE_ZOOM_TEXT, caption,
                           COLUMN_TEXTURE_ZOOM_SENSITIVE, FALSE,
                           -1);
        free(caption);
    }
    for (i = 1; i <= 5; i++)
    {
        gchar *caption;

        bugle_asprintf(&caption, "%d:1", (1 << i));
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_TEXTURE_ZOOM_VALUE, (gdouble) (1 << i),
                           COLUMN_TEXTURE_ZOOM_TEXT, caption,
                           COLUMN_TEXTURE_ZOOM_SENSITIVE, FALSE,
                           -1);
        free(caption);
    }

    context->texture.zoom = zoom = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(zoom), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(zoom), cell,
                                   "text", COLUMN_TEXTURE_ZOOM_TEXT,
                                   "sensitive", COLUMN_TEXTURE_ZOOM_SENSITIVE,
                                   NULL);
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(zoom),
                                         texture_zoom_row_separator,
                                         NULL, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(zoom), 0);
    g_signal_connect(G_OBJECT(zoom), "changed",
                     G_CALLBACK(texture_zoom_changed), context);
    g_object_unref(G_OBJECT(store));
    return zoom;
}

static GtkWidget *build_texture_page_select(GldbWindow *context)
{
    GtkWidget *select;
    GtkWidget *label, *id, *level, *zoom;

    select = gtk_hbox_new(FALSE, 0);

    label = gtk_label_new(_("Texture"));
    id = build_texture_page_id(context);
    gtk_box_pack_start(GTK_BOX(select), label, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(select), id, FALSE, FALSE, 0);

    label = gtk_label_new(_("Level"));
    level = build_texture_page_level(context);
    gtk_box_pack_start(GTK_BOX(select), label, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(select), level, FALSE, FALSE, 0);

    label = gtk_label_new(_("Zoom"));
    zoom = build_texture_page_zoom(context);
    gtk_box_pack_start(GTK_BOX(select), label, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(select), zoom, FALSE, FALSE, 0);

    return select;
}

static GtkWidget *build_texture_page_mag_filter(GldbWindow *context)
{
    GtkCellRenderer *cell;
    GtkWidget *filter;

    context->texture.mag_filter = filter = gtk_combo_box_new_with_model(texture_mag_filters);
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(filter), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(filter), cell,
                                   "text", COLUMN_TEXTURE_FILTER_TEXT, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter), 0);
    return filter;
}

static GtkWidget *build_texture_page_min_filter(GldbWindow *context)
{
    GtkCellRenderer *cell;
    GtkWidget *filter;

    context->texture.min_filter = filter = gtk_combo_box_new_with_model(texture_min_filters);
    context->texture.min_filter_cell = cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(filter), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(filter), cell,
                                   "text", COLUMN_TEXTURE_FILTER_TEXT,
                                   "sensitive", COLUMN_TEXTURE_FILTER_NON_MIP,
                                   NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter), 0);
    return filter;
}

static GtkWidget *build_texture_page_view(GldbWindow *context)
{
    GtkWidget *view;
    GtkWidget *label, *mag, *min;

    view = gtk_hbox_new(FALSE, 0);

    label = gtk_label_new(_("Mag filter"));
    mag = build_texture_page_mag_filter(context);
    gtk_box_pack_start(GTK_BOX(view), label, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(view), mag, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(mag), "changed",
                     G_CALLBACK(texture_filter_changed), context);

    label = gtk_label_new(_("Min filter"));
    min = build_texture_page_min_filter(context);
    gtk_box_pack_start(GTK_BOX(view), label, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(view), min, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(min), "changed",
                     G_CALLBACK(texture_filter_changed), context);

    return view;
}

static GtkWidget *build_texture_page_toolbar(GldbWindow *context)
{
    GtkWidget *toolbar;
    GtkToolItem *item;

    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

    item = gtk_tool_button_new_from_stock(GTK_STOCK_COPY);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(texture_copy_clicked), context);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    context->texture.zoom_in = item = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(texture_zoom_in_clicked), context);
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    context->texture.zoom_out = item = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(texture_zoom_out_clicked), context);
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    context->texture.zoom_100 = item = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_100);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(texture_zoom_100_clicked), context);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_FIT);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(texture_zoom_fit_clicked), context);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    return toolbar;
}

static GtkWidget *build_texture_page_draw(GldbWindow *context)
{
    GtkWidget *alignment, *aspect, *draw;
    GdkGLConfig *glconfig;
    static const int attrib_list[] =
    {
        GDK_GL_USE_GL, TRUE,
        GDK_GL_RGBA, TRUE,
        GDK_GL_DOUBLEBUFFER, TRUE,
        GDK_GL_ATTRIB_LIST_NONE
    };

    /* FIXME: use new_for_screen */
    glconfig = gdk_gl_config_new(attrib_list);
    g_return_val_if_fail(glconfig != NULL, NULL);

    draw = gtk_drawing_area_new();
    gtk_widget_set_size_request(draw, 1, 1);
    gtk_widget_set_gl_capability(draw, glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);
    g_signal_connect_after(G_OBJECT(draw), "realize",
                           G_CALLBACK(texture_draw_realize), context);
    g_signal_connect(G_OBJECT(draw), "configure-event",
                     G_CALLBACK(texture_draw_configure), context);
    g_signal_connect(G_OBJECT(draw), "expose-event",
                     G_CALLBACK(texture_draw_expose), context);

    aspect = gtk_aspect_frame_new(NULL, 0.5, 0.5, 1.0, TRUE);
    gtk_frame_set_shadow_type(GTK_FRAME(aspect), GTK_SHADOW_NONE);
    alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_container_add(GTK_CONTAINER(aspect), draw);
    gtk_container_add(GTK_CONTAINER(alignment), aspect);

    context->texture.draw = draw;
    return alignment;
}

static void build_texture_page(GldbWindow *context)
{
    GtkWidget *label, *draw, *toolbar;
    GtkWidget *vbox, *select, *view, *scrolled;
    gint page;

    select = build_texture_page_select(context);
    view = build_texture_page_view(context);
    toolbar = build_texture_page_toolbar(context);
    draw = build_texture_page_draw(context);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    if (draw)
    {
        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
                                              draw);
    }
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), select, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), view, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    label = gtk_label_new(_("Textures"));
    page = gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook),
                                    vbox, label);
    context->texture.dirty = false;
    context->texture.display_target = GL_NONE;
    context->texture.width = 64;
    context->texture.height = 64;
    context->texture.page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(context->notebook), page);
    g_signal_connect(G_OBJECT(context->texture.page), "expose-event",
                     G_CALLBACK(texture_expose), context);
}
#endif /* HAVE_GTKGLEXT */

#ifdef GLDB_GUI_SHADER
static void update_shader_ids(GldbWindow *context, GLenum target)
{
    const gldb_state *s, *t, *u;
    GtkTreeModel *model;
    GtkTreeIter iter, old_iter;
    guint old;
    gboolean have_old = FALSE, have_old_iter = FALSE;
    bugle_list_node *nt;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->shader.id));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->shader.id), &iter))
    {
        have_old = TRUE;
        gtk_tree_model_get(model, &iter, 0, &old, -1);
    }
    gtk_list_store_clear(GTK_LIST_STORE(model));

    s = gldb_state_update(); /* FIXME: manage state tree ourselves */
    switch (target)
    {
#ifdef GL_ARB_vertex_program
    case GL_VERTEX_PROGRAM_ARB:
#endif
#ifdef GL_ARB_fragment_program
    case GL_FRAGMENT_PROGRAM_ARB:
#endif
#ifdef GLDB_GUI_SHADER_OLD
        s = state_find_child_enum(s, target);
        if (!s) return;
        for (nt = bugle_list_head(&s->children); nt != NULL; nt = bugle_list_next(nt))
        {
            t = (const gldb_state *) bugle_list_data(nt);
            if (t->enum_name == 0 && t->name[0] >= '0' && t->name[0] <= '9')
            {
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   0, (guint) t->numeric_name, -1);
                if (have_old && t->numeric_name == (GLint) old)
                {
                    old_iter = iter;
                    have_old_iter = TRUE;
                }
            }
        }
        break;
#endif
#ifdef GL_ARB_vertex_shader
    case GL_VERTEX_SHADER_ARB:
#endif
#ifdef GL_ARB_fragment_shader
    case GL_FRAGMENT_SHADER_ARB:
#endif
#ifdef GLDB_GUI_SHADER_GLSL_ARB
        for (nt = bugle_list_head(&s->children); nt != NULL; nt = bugle_list_next(nt))
        {
            const char *target_string = "";
            switch (target)
            {
#ifdef GL_ARB_vertex_shader
            case GL_VERTEX_SHADER_ARB: target_string = "GL_VERTEX_SHADER"; break;
#endif
#ifdef GL_ARB_fragment_shader
            case GL_FRAGMENT_SHADER_ARB: target_string = "GL_FRAGMENT_SHADER"; break;
#endif
            }
            t = (const gldb_state *) bugle_list_data(nt);
            u = state_find_child_enum(t, GL_OBJECT_SUBTYPE_ARB);
            if (u && !strcmp(u->value, target_string))
            {
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   0, (guint) t->numeric_name, -1);
                if (have_old && t->numeric_name == (GLint) old)
                {
                    old_iter = iter;
                    have_old_iter = TRUE;
                }
            }
        }
        break;
#endif
    }
    if (have_old_iter)
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(context->shader.id),
                                      &old_iter);
    else
        gtk_combo_box_set_active(GTK_COMBO_BOX(context->shader.id), 0);
}

static void shader_target_changed(GtkComboBox *target_box, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint target;
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    model = gtk_combo_box_get_model(target_box);
    if (!gtk_combo_box_get_active_iter(target_box, &iter)) return;
    gtk_tree_model_get(model, &iter, 1, &target, -1);

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
        update_shader_ids(context, target);
    else if (gtk_combo_box_get_active(GTK_COMBO_BOX(context->shader.id)) == -1)
        gtk_combo_box_set_active(GTK_COMBO_BOX(context->shader.id), 0);
}

static void shader_id_changed(GtkComboBox *id_box, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint target, id;
    GldbWindow *context;

    context = (GldbWindow *) user_data;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->shader.target));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->shader.target), &iter)) return;
    gtk_tree_model_get(model, &iter, 1, &target, -1);

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->shader.id));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->shader.id), &iter)) return;
    gtk_tree_model_get(model, &iter, 0, &id, -1);

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
    {
        set_response_handler(context, seq, response_callback_shader, NULL);
        gldb_send_data_shader(seq++, id, target);
    }
}

static GtkWidget *build_shader_page_target(GldbWindow *context)
{
    GtkListStore *store;
    GtkWidget *target;
    GtkCellRenderer *cell;
    GtkTreeIter iter;

    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_UINT);
#ifdef GL_ARB_vertex_program
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       0, "GL_VERTEX_PROGRAM_ARB",
                       1, (gint) GL_VERTEX_PROGRAM_ARB, -1);
#endif
#ifdef GL_ARB_fragment_program
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       0, "GL_FRAGMENT_PROGRAM_ARB",
                       1, (gint) GL_FRAGMENT_PROGRAM_ARB, -1);
#endif
#ifdef GL_ARB_vertex_shader
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       0, "GL_VERTEX_SHADER_ARB",
                       1, (gint) GL_VERTEX_SHADER_ARB, -1);
#endif
#ifdef GL_ARB_fragment_shader
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       0, "GL_FRAGMENT_SHADER_ARB",
                       1, (gint) GL_FRAGMENT_SHADER_ARB, -1);
#endif
    target = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_combo_box_set_active(GTK_COMBO_BOX(target), 0);
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(target), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(target), cell,
                                   "text", 0, NULL);
    g_signal_connect(G_OBJECT(target), "changed",
                     G_CALLBACK(shader_target_changed), context);
    g_object_unref(G_OBJECT(store));

    context->shader.target = target;
    return target;
}

static GtkWidget *build_shader_page_id(GldbWindow *context)
{
    GtkListStore *store;
    GtkWidget *id;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(1, G_TYPE_UINT);
    id = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(id), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(id), cell,
                                   "text", 0, NULL);
    g_signal_connect(G_OBJECT(id), "changed",
                     G_CALLBACK(shader_id_changed), context);
    g_object_unref(G_OBJECT(store));

    context->shader.id = id;
    return id;
}

static void build_shader_page(GldbWindow *context)
{
    GtkWidget *label, *vbox, *hbox, *target, *id, *source, *scrolled;
    gint page;

    target = build_shader_page_target(context);
    id = build_shader_page_id(context);
    source = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(source), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(source), FALSE);
    gtk_text_buffer_create_tag(gtk_text_view_get_buffer(GTK_TEXT_VIEW(source)),
                               "lineno",
                               "foreground", "grey",
                               NULL);
    gtk_text_buffer_create_tag(gtk_text_view_get_buffer(GTK_TEXT_VIEW(source)),
                               "default",
                               "family", "Monospace",
                               NULL);
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), source);

    vbox = gtk_vbox_new(FALSE, 0);
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), target, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), id, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);

    label = gtk_label_new(_("Shaders"));
    page = gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook), hbox, label);
    context->shader.dirty = false;
    context->shader.source = source;
    context->shader.page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(context->notebook), page);
    g_signal_connect(G_OBJECT(context->shader.page), "expose-event",
                     G_CALLBACK(shader_expose), context);
}
#endif /* shaders */

static void build_backtrace_page(GldbWindow *context)
{
    GtkWidget *label, *view, *scrolled;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;
    gint page;

    context->backtrace.backtrace_store = gtk_list_store_new(1, G_TYPE_STRING);
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Call"),
                                                      cell,
                                                      "text", 0,
                                                      NULL);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(context->backtrace.backtrace_store));
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), false);
    gtk_widget_show(view);
    g_object_unref(G_OBJECT(context->backtrace.backtrace_store)); /* So that it dies with the view */

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), view);

    label = gtk_label_new(_("Backtrace"));
    page = gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook), scrolled, label);
    context->backtrace.dirty = false;
    context->backtrace.page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(context->notebook), page);
    g_signal_connect(G_OBJECT(context->backtrace.page), "expose-event",
                     G_CALLBACK(backtrace_expose), context);
}

static const gchar *ui_desc =
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Quit' />"
"    </menu>"
"    <menu action='OptionsMenu'>"
"      <menuitem action='Chain' />"
"    </menu>"
"    <menu action='RunMenu'>"
"      <menuitem action='Run' />"
"      <menuitem action='Stop' />"
"      <menuitem action='Continue' />"
"      <menuitem action='Kill' />"
"      <separator />"
"      <menuitem action='Breakpoints' />"
"    </menu>"
"  </menubar>"
"</ui>";

static GtkActionEntry action_desc[] =
{
    { "FileMenu", NULL, "_File", NULL, NULL, NULL },
    { "Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q", NULL, G_CALLBACK(quit_action) },

    { "OptionsMenu", NULL, "_Options", NULL, NULL, NULL },
    { "Chain", NULL, "Filter _Chain", NULL, NULL, G_CALLBACK(chain_action) },

    { "RunMenu", NULL, "_Run", NULL, NULL, NULL },
    { "Breakpoints", NULL, "_Breakpoints...", NULL, NULL, G_CALLBACK(breakpoints_action) }
};

static GtkActionEntry running_action_desc[] =
{
    { "Stop", NULL, "_Stop", "<control>Break", NULL, G_CALLBACK(stop_action) }
};

static GtkActionEntry stopped_action_desc[] =
{
    { "Continue", NULL, "_Continue", "<control>F9", NULL, G_CALLBACK(continue_action) },
    { "Kill", NULL, "_Kill", "<control>F2", NULL, G_CALLBACK(kill_action) }
};

static GtkActionEntry dead_action_desc[] =
{
    { "Run", NULL, "_Run", NULL, NULL, G_CALLBACK(run_action) }
};

static GtkUIManager *build_ui(GldbWindow *context)
{
    GtkUIManager *ui;
    GError *error = NULL;

    context->actions = gtk_action_group_new("Actions");
    gtk_action_group_add_actions(context->actions, action_desc,
                                 G_N_ELEMENTS(action_desc), context);
    context->running_actions = gtk_action_group_new("RunningActions");
    gtk_action_group_add_actions(context->running_actions, running_action_desc,
                                 G_N_ELEMENTS(running_action_desc), context);
    context->stopped_actions = gtk_action_group_new("StoppedActions");
    gtk_action_group_add_actions(context->stopped_actions, stopped_action_desc,
                                 G_N_ELEMENTS(stopped_action_desc), context);
    context->dead_actions = gtk_action_group_new("DeadActions");
    gtk_action_group_add_actions(context->dead_actions, dead_action_desc,
                                 G_N_ELEMENTS(dead_action_desc), context);
    gtk_action_group_set_sensitive(context->running_actions, FALSE);
    gtk_action_group_set_sensitive(context->stopped_actions, FALSE);

    ui = gtk_ui_manager_new();
    gtk_ui_manager_set_add_tearoffs(ui, TRUE);
    gtk_ui_manager_insert_action_group(ui, context->actions, 0);
    gtk_ui_manager_insert_action_group(ui, context->running_actions, 0);
    gtk_ui_manager_insert_action_group(ui, context->stopped_actions, 0);
    gtk_ui_manager_insert_action_group(ui, context->dead_actions, 0);
    if (!gtk_ui_manager_add_ui_from_string(ui, ui_desc, strlen(ui_desc), &error))
    {
        g_message(_("Failed to create UI: %s"), error->message);
        g_error_free(error);
    }
    return ui;
}

static void build_main_window(GldbWindow *context)
{
    GtkUIManager *ui;
    GtkWidget *vbox;

    bugle_list_init(&context->response_handlers, TRUE);

    context->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(context->window), "gldb-gui");
    g_signal_connect(G_OBJECT(context->window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    ui = build_ui(context);
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_ui_manager_get_widget(ui, "/MenuBar"),
                       FALSE, FALSE, 0);

    context->breakpoints_store = gtk_list_store_new(1, G_TYPE_STRING);
    context->notebook = gtk_notebook_new();
    build_state_page(context);
#if HAVE_GTKGLEXT
    build_texture_page(context);
#endif
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    build_shader_page(context);
#endif
    build_backtrace_page(context);
    gtk_box_pack_start(GTK_BOX(vbox), context->notebook, TRUE, TRUE, 0);

    context->statusbar = gtk_statusbar_new();
    context->statusbar_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(context->statusbar), _("Program status"));
    gtk_statusbar_push(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id, _("Not running"));
    gtk_box_pack_start(GTK_BOX(vbox), context->statusbar, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(context->window), vbox);
    gtk_window_set_default_size(GTK_WINDOW(context->window), 640, 480);
    gtk_window_add_accel_group (GTK_WINDOW(context->window),
                                gtk_ui_manager_get_accel_group(ui));
    gtk_widget_show_all(context->window);
}

static void build_function_names(void)
{
    gsize i;
    GtkTreeIter iter;

    function_names = gtk_list_store_new(1, G_TYPE_STRING);
    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
    {
        gtk_list_store_append(function_names, &iter);
        gtk_list_store_set(function_names, &iter, 0, budgie_function_names[i], -1);
    }
}

#if HAVE_GTKGLEXT
static GtkListStore *build_texture_filters(gboolean min_filter)
{
    GtkListStore *store;
    GtkTreeIter iter;
    int count, i;
    const struct
    {
        GLuint value;
        const gchar *text;
        gboolean non_mip;
    } filters[6] =
    {
        { GL_NEAREST,                "GL_NEAREST",                TRUE },
        { GL_LINEAR,                 "GL_LINEAR",                 TRUE },
        { GL_NEAREST_MIPMAP_NEAREST, "GL_NEAREST_MIPMAP_NEAREST", FALSE },
        { GL_LINEAR_MIPMAP_NEAREST,  "GL_LINEAR_MIPMAP_NEAREST",  FALSE },
        { GL_NEAREST_MIPMAP_LINEAR,  "GL_NEAREST_MIPMAP_LINEAR",  FALSE },
        { GL_LINEAR_MIPMAP_LINEAR,   "GL_LINEAR_MIPMAP_LINEAR",   FALSE }
    };

    store = gtk_list_store_new(4, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
    count = min_filter ? 6 : 2;
    for (i = 0; i < count; i++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_TEXTURE_FILTER_VALUE, filters[i].value,
                           COLUMN_TEXTURE_FILTER_TEXT, filters[i].text,
                           COLUMN_TEXTURE_FILTER_TRUE, TRUE,
                           COLUMN_TEXTURE_FILTER_NON_MIP, filters[i].non_mip,
                           -1);
    }
    return store;
}
#endif

int main(int argc, char **argv)
{
    GldbWindow context;

    gtk_init(&argc, &argv);
#if HAVE_GTKGLEXT
    gtk_gl_init(&argc, &argv);
    texture_mag_filters = GTK_TREE_MODEL(build_texture_filters(FALSE));
    texture_min_filters = GTK_TREE_MODEL(build_texture_filters(TRUE));
#endif
    gldb_initialise(argc, argv);
    bugle_initialise_hashing();

    build_function_names();
    memset(&context, 0, sizeof(context));
    build_main_window(&context);

    gtk_main();

    gldb_shutdown();
    g_object_unref(G_OBJECT(function_names));
#if HAVE_GTKGLEXT
    g_object_unref(G_OBJECT(texture_mag_filters));
    g_object_unref(G_OBJECT(texture_min_filters));
#endif
    g_object_unref(G_OBJECT(context.breakpoints_store));
    return 0;
}
