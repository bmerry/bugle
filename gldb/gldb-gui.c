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
/* FIXME: Not sure if this is the correct definition of GETTEXT_PACKAGE... */
#ifndef GETTEXT_PACKAGE
# define GETTEXT_PACKAGE "bugle00"
#endif
#if HAVE_GTKGLEXT
# include "glee/GLee.h"
#endif
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
#include "gldb/gldb-channels.h"
#include "gldb/gldb-gui-image.h"

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

#if HAVE_GTKGLEXT
enum
{
    COLUMN_TEXTURE_ID_ID,
    COLUMN_TEXTURE_ID_TARGET,
    COLUMN_TEXTURE_ID_LEVELS,
    COLUMN_TEXTURE_ID_CHANNELS,
    COLUMN_TEXTURE_ID_TEXT
};

enum
{
    COLUMN_FRAMEBUFFER_ID_ID,
    COLUMN_FRAMEBUFFER_ID_TARGET,
    COLUMN_FRAMEBUFFER_ID_TEXT
};

enum
{
    COLUMN_FRAMEBUFFER_BUFFER_ID,
    COLUMN_FRAMEBUFFER_BUFFER_CHANNELS,
    COLUMN_FRAMEBUFFER_BUFFER_TEXT
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
    GtkWidget *id, *level;

    GldbGuiImageViewer *viewer;
    GldbGuiImage active, progressive;
} GldbWindowTexture;

typedef struct
{
    bool dirty;
    GtkWidget *page;
    GtkWidget *id, *buffer;

    GldbGuiImage active;
    GldbGuiImageViewer *viewer;
} GldbWindowFramebuffer;
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
    GldbWindowFramebuffer framebuffer;
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
#define TEXTURE_CALLBACK_FLAG_FIRST 1
#define TEXTURE_CALLBACK_FLAG_LAST 2

typedef struct
{
    GLenum target;
    GLenum face;
    GLuint level;
    uint32_t channels;
    guint32 flags;

    /* The following attributes are set for the first of a set, to allow
     * the image to be suitably allocated
     */
    GldbGuiImageType type;
    int nlevels;
    int nplanes;
} texture_callback_data;

typedef struct
{
    uint32_t channels;
    guint32 flags;  /* not used at present */
} framebuffer_callback_data;

#endif /* HAVE_GTKGLEXT */

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

static void state_update(GldbWindow *context)
{
    if (!context->state.dirty) return;
    context->state.dirty = FALSE;

    update_state_r(gldb_state_update(), context->state.state_store, NULL);
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

/* Saves some of the current state of a combo box into the given array.
 * This array can then be passed to combo_box_restore_old, which will attempt
 * to find an entry that has the same attributes and will then set that
 * element in the combo box. If there is no current value, the GValue
 * elements are unset. The variadic arguments are the column IDs to use,
 * followed by -1. The array must have enough storage.
 *
 * Currently these function are limited to 4 identifying columns, and
 * to guint, gint and gdouble types.
 */
static void combo_box_get_old(GtkComboBox *box, GValue *save, ...)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    va_list ap;
    int col;
    gboolean have_iter;

    va_start(ap, save);
    model = gtk_combo_box_get_model(box);
    have_iter = gtk_combo_box_get_active_iter(box, &iter);
    while ((col = va_arg(ap, int)) != -1)
    {
        memset(save, 0, sizeof(GValue));
        if (have_iter)
            gtk_tree_model_get_value(model, &iter, col, save);
        save++;
    }
    va_end(ap);
}

/* Note: this function automatically releases the GValues, so it is
 * important to match with the previous one, or else release values
 * manually.
 */
static void combo_box_restore_old(GtkComboBox *box, GValue *save, ...)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GValue cur;
    va_list ap;
    int col, cols[4];
    int ncols = 0;
    int i;
    gboolean more, match;

    va_start(ap, save);
    while ((col = va_arg(ap, int)) != -1)
    {
        g_return_if_fail(ncols < 4);
        cols[ncols++] = col;
    }
    va_end(ap);

    if (!G_IS_VALUE(&save[0]))
    {
        gtk_combo_box_set_active(box, 0);
        return;
    }

    memset(&cur, 0, sizeof(cur));
    model = gtk_combo_box_get_model(box);
    more = gtk_tree_model_get_iter_first(model, &iter);
    while (more)
    {
        match = TRUE;
        for (i = 0; i < ncols && match; i++)
        {
            gtk_tree_model_get_value(model, &iter, cols[i], &cur);
            switch (gtk_tree_model_get_column_type(model, cols[i]))
            {
            case G_TYPE_INT:
                match = g_value_get_int(&save[i]) == g_value_get_int(&cur);
                break;
            case G_TYPE_UINT:
                match = g_value_get_uint(&save[i]) == g_value_get_uint(&cur);
                break;
            case G_TYPE_DOUBLE:
                match = g_value_get_double(&save[i]) == g_value_get_double(&cur);
                break;
            default:
                g_return_if_reached();
            }
            g_value_unset(&cur);
        }
        if (match)
        {
            for (i = 0; i < ncols; i++)
                g_value_unset(&save[i]);
            gtk_combo_box_set_active_iter(box, &iter);
            return;
        }

        more = gtk_tree_model_iter_next(model, &iter);
    }
    gtk_combo_box_set_active(box, 0);
}

#if HAVE_GTKGLEXT

#if 0 /* FIXME: incorporate into image viewer code */
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
#endif /* 0 */

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
    GLenum format;
    uint32_t channels;
    GldbGuiImageLevel *level;
    int i;

    r = (gldb_response_data_texture *) response;
    data = (texture_callback_data *) user_data;
    if (data->flags & TEXTURE_CALLBACK_FLAG_FIRST)
    {
        gldb_gui_image_clear(&context->texture.progressive);
        gldb_gui_image_allocate(&context->texture.progressive, data->type,
                                data->nlevels, data->nplanes);
        context->texture.viewer->current = NULL;
    }

    if (response->code != RESP_DATA || !r->length)
    {
        /* FIXME: tag the texture as invalid and display error */
    }
    else
    {
        GdkGLContext *glcontext;
        GdkGLDrawable *gldrawable;
        GLuint texture_width, texture_height, texture_depth, plane;
        GLenum face;

        channels = data->channels;
        format = gldb_channel_get_display_token(channels);
        glcontext = gtk_widget_get_gl_context(context->texture.viewer->draw);
        gldrawable = gtk_widget_get_gl_drawable(context->texture.viewer->draw);

        /* FIXME: check runtime support for all extensions */
        if (gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        {
#ifdef GL_ARB_texture_non_power_of_two
            if (gdk_gl_query_gl_extension("GL_ARB_texture_non_power_of_two"))
            {
                texture_width = r->width;
                texture_height = r->height;
                texture_depth = r->depth;
            }
            else
#endif
            {
                texture_width = texture_height = texture_depth = 1;
                while (texture_width < r->width) texture_width *= 2;
                while (texture_height < r->height) texture_height *= 2;
                while (texture_depth < r->depth) texture_depth *= 2;
            }

            face = context->texture.progressive.texture_target;
            plane = 0;
            level = &context->texture.progressive.levels[data->level];
            switch (context->texture.progressive.type)
            {
#ifdef GL_EXT_texture_cube_map
            case GLDB_GUI_IMAGE_TYPE_CUBE:
                plane = data->face - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                face = data->face;
                /* Fall through */
                /* FIXME: test for cube map extension first */
#endif
            case GLDB_GUI_IMAGE_TYPE_2D:
                glTexImage2D(face, data->level, format,
                             texture_width, texture_height, 0,
                             format, GL_FLOAT, NULL);
                glTexSubImage2D(face, data->level, 0, 0,
                                r->width, r->height, format, GL_FLOAT, r->data);
                level->planes[plane].width = r->width;
                level->planes[plane].height = r->height;
                level->planes[plane].channels = data->channels;
                level->planes[plane].owns_pixels = true;
                level->planes[plane].pixels = (GLfloat *) r->data;
                if (data->flags & TEXTURE_CALLBACK_FLAG_FIRST)
                {
                    context->texture.progressive.s = (GLfloat) r->width / texture_width;
                    context->texture.progressive.t = (GLfloat) r->height / texture_height;
                }
                break;
#ifdef GL_EXT_texture3D
            case GL_TEXTURE_3D_EXT:
                if (gdk_gl_query_gl_extension("GL_EXT_texture3D"))
                {
                    glTexImage3DEXT(face, data->level, format,
                                    texture_width, texture_height, texture_depth, 0,
                                    format, GL_FLOAT, NULL);
                    glTexSubImage3DEXT(face, data->level, 0, 0, 0,
                                       r->width, r->height, r->depth,
                                       format, GL_FLOAT, r->data);
                    level->nplanes = r->depth;
                    level->planes = bugle_calloc(r->depth, sizeof(GldbGuiImagePlane));
                    for (plane = 0; plane < r->depth; plane++)
                    {
                        level->planes[plane].width = r->width;
                        level->planes[plane].height = r->width;
                        level->planes[plane].channels = data->channels;
                        level->planes[plane].owns_pixels = (plane == 0);
                        level->planes[plane].pixels = ((GLfloat *) r->data) + r->width * r->height * gldb_channel_count(data->channels);
                    }
                    break;
                }
#endif
            default:
                g_error("Image type is not supported.");
            }
            gdk_gl_drawable_gl_end(gldrawable);
        }

        r->data = NULL; /* Prevents gldb_free_response from freeing the data */
    }

    if (data->flags & TEXTURE_CALLBACK_FLAG_LAST)
    {
        gldb_gui_image_clear(&context->texture.active);
        context->texture.active = context->texture.progressive;
        context->texture.viewer->current = &context->texture.active;
        memset(&context->texture.progressive, 0, sizeof(context->texture.progressive));

#ifdef GL_NV_texture_rectangle
        gldb_gui_image_viewer_update_min_filter(context->texture.viewer,
                                                data->target != GL_TEXTURE_RECTANGLE_NV);
#endif
        gldb_gui_image_viewer_update_levels(context->texture.viewer);
        gtk_widget_queue_draw(context->texture.viewer->draw);
    }

    gldb_free_response(response);
    free(data);
    return TRUE;
}

static void update_texture_ids(GldbWindow *context);

static void texture_update(GldbWindow *context)
{
    if (!context->texture.dirty) return;
    context->texture.dirty = FALSE;

    /* Simply invoke a change event on the selector. This launches a
     * cascade of updates.
     */
    update_texture_ids(context);
}

/***********************************************************************/

static gboolean response_callback_framebuffer(GldbWindow *context,
                                              gldb_response *response,
                                              gpointer user_data)
{
    gldb_response_data_framebuffer *r;
    framebuffer_callback_data *data;
    GLenum format;
    uint32_t channels;
    GLint width, height, texture_width, texture_height;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean more, valid;

    r = (gldb_response_data_framebuffer *) response;

    gldb_gui_image_clear(&context->framebuffer.active);
    context->framebuffer.viewer->current = NULL;
    if (response->code != RESP_DATA || !r->length)
    {
        /* FIXME: tag the framebuffer as invalid and display error */
    }
    else
    {
        GdkGLContext *glcontext;
        GdkGLDrawable *gldrawable;

        data = (framebuffer_callback_data *) user_data;
        channels = data->channels;
        format = gldb_channel_get_display_token(channels);
        width = r->width;
        height = r->height;

        glcontext = gtk_widget_get_gl_context(context->framebuffer.viewer->draw);
        gldrawable = gtk_widget_get_gl_drawable(context->framebuffer.viewer->draw);
        /* FIXME: check runtime support for all extensions */
        if (gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        {
            gldb_gui_image_allocate(&context->framebuffer.active,
                                    GLDB_GUI_IMAGE_TYPE_2D, 1, 1);
#ifdef GL_ARB_texture_non_power_of_two
            if (gdk_gl_query_gl_extension("GL_ARB_texture_non_power_of_two"))
            {
                texture_width = width;
                texture_height = height;
            }
            else
#endif
            {
                texture_width = texture_height = 1;
                while (texture_width < width) texture_width *= 2;
                while (texture_height < height) texture_height *= 2;
            }

            /* Allow NPOT framebuffers on POT textures by filling into one corner */
            /* FIXME: create mip levels for zoom-out */
            glTexImage2D(GL_TEXTURE_2D, 0, format, texture_width, texture_height, 0, format, GL_FLOAT, NULL);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_FLOAT, r->data);
            gdk_gl_drawable_gl_end(gldrawable);
        }

        /* Save our copy of the data */
        context->framebuffer.active.s = (GLfloat) width / texture_width;
        context->framebuffer.active.t = (GLfloat) height / texture_height;
        context->framebuffer.active.levels[0].planes[0].width = width;
        context->framebuffer.active.levels[0].planes[0].height = height;
        context->framebuffer.active.levels[0].planes[0].channels = channels;
        context->framebuffer.active.levels[0].planes[0].owns_pixels = true;
        context->framebuffer.active.levels[0].planes[0].pixels = (GLfloat *) r->data;
        r->data = NULL; /* stops gldb_free_response from freeing data */

        /* Prepare for rendering */
        context->framebuffer.viewer->current = &context->framebuffer.active;
        context->framebuffer.viewer->current_level = 0;
        context->framebuffer.viewer->current_plane = 0;
    }

    gldb_gui_image_viewer_update_zoom(context->framebuffer.viewer);
    gtk_widget_queue_draw(context->framebuffer.viewer->draw);
    gldb_free_response(response);
    return TRUE;
}

static void update_framebuffer_ids(GldbWindow *context);

static void framebuffer_update(GldbWindow *context)
{
    if (!context->framebuffer.dirty) return;
    context->framebuffer.dirty = FALSE;

    /* Simply invoke a change event on the selector. This launches a
     * cascade of updates.
     */
    update_framebuffer_ids(context);
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
        GtkTextIter start, end;

        gtk_text_buffer_set_text(buffer, r->data, r->length);
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        gtk_text_buffer_apply_tag_by_name(buffer, "default", &start, &end);
    }
    gldb_free_response(response);
    return TRUE;
}

static void shader_update(GldbWindow *context)
{
    if (!context->shader.dirty) return;
    context->shader.dirty = FALSE;

    /* Simply invoke a change event on the target. This launches a
     * cascade of updates.
     */
    g_signal_emit_by_name(G_OBJECT(context->shader.target), "changed", NULL, NULL);
}
#endif

static void backtrace_update(GldbWindow *context)
{
    gchar *argv[4];
    gint in, out;
    GError *error = NULL;
    GIOChannel *inf, *outf;
    gchar *line;
    gsize terminator;
    const gchar *cmds = "set width 0\nset height 0\nbacktrace\nquit\n";

    if (!context->backtrace.dirty) return;
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
}

static void notebook_update(GldbWindow *context, gint new_page)
{
    gint page;
    GtkNotebook *notebook;

    notebook = GTK_NOTEBOOK(context->notebook);
    if (new_page >= 0) page = new_page;
    else page = gtk_notebook_get_current_page(notebook);
    if (page == gtk_notebook_page_num(notebook, context->state.page))
        state_update(context);
#if HAVE_GTKGLEXT
    else if (page == gtk_notebook_page_num(notebook, context->texture.page))
        texture_update(context);
    else if (page == gtk_notebook_page_num(notebook, context->framebuffer.page))
        framebuffer_update(context);
#endif
#ifdef GLDB_GUI_SHADER
    else if (page == gtk_notebook_page_num(notebook, context->shader.page))
        shader_update(context);
#endif
    else if (page == gtk_notebook_page_num(notebook, context->backtrace.page))
        backtrace_update(context);
}

static void notebook_switch_page(GtkNotebook *notebook,
                                 GtkNotebookPage *page,
                                 guint page_num,
                                 gpointer user_data)
{
    notebook_update((GldbWindow *) user_data, page_num);
}

static void update_status_bar(GldbWindow *context, const gchar *text)
{
    gtk_statusbar_pop(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id);
    gtk_statusbar_push(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id, text);
}

static void stopped(GldbWindow *context, const gchar *text)
{
    context->state.dirty = true;
#if HAVE_GTKGLEXT
    context->texture.dirty = true;
    context->framebuffer.dirty = true;
#endif
#ifdef GLDB_GUI_SHADER
    context->shader.dirty = true;
#endif
    context->backtrace.dirty = true;
    gtk_action_group_set_sensitive(context->running_actions, FALSE);
    gtk_action_group_set_sensitive(context->stopped_actions, TRUE);

    notebook_update(context, -1);
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
    gboolean done = FALSE;
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

static void step_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    gtk_action_group_set_sensitive(context->stopped_actions, FALSE);
    gtk_action_group_set_sensitive(context->running_actions, TRUE);
    gldb_send_step(seq++);
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
}

#if HAVE_GTKGLEXT
static void update_texture_ids(GldbWindow *context)
{
    GValue old[2];
    const gldb_state *root, *s, *t, *l, *f, *param;
    GtkTreeModel *model;
    GtkTreeIter iter;
    bugle_list_node *nt, *nl;
    gchar *name;
    guint levels;
    uint32_t channels;

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

    combo_box_get_old(GTK_COMBO_BOX(context->texture.id), old,
                      COLUMN_TEXTURE_ID_ID, COLUMN_TEXTURE_ID_TARGET, -1);
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->texture.id));
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
                channels = 0;
                for (nl = bugle_list_head(&f->children); nl != NULL; nl = bugle_list_next(nl))
                {
                    l = (const gldb_state *) bugle_list_data(nl);
                    if (l->enum_name == 0)
                    {
                        int i;

                        levels = MAX(levels, (guint) l->numeric_name + 1);
                        for (i = 0; gldb_channel_table[i].channel; i++)
                            if (gldb_channel_table[i].texture_size_token
                                && (param = state_find_child_enum(l, gldb_channel_table[i].texture_size_token)) != NULL
                                && strcmp(param->value, "0") != 0)
                            {
                                channels |= gldb_channel_table[i].channel;
                            }
                    }
                }
                channels = gldb_channel_get_query_channels(channels);
                if (!levels || !channels) continue;
                bugle_asprintf(&name, "%u (%s)", (unsigned int) t->numeric_name, target_names[trg]);
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COLUMN_TEXTURE_ID_ID, (guint) t->numeric_name,
                                   COLUMN_TEXTURE_ID_TARGET, (guint) targets[trg],
                                   COLUMN_TEXTURE_ID_LEVELS, levels,
                                   COLUMN_TEXTURE_ID_CHANNELS, channels,
                                   COLUMN_TEXTURE_ID_TEXT, name,
                                   -1);
                free(name);
            }
        }
    }

    combo_box_restore_old(GTK_COMBO_BOX(context->texture.id), old,
                          COLUMN_TEXTURE_ID_ID, COLUMN_TEXTURE_ID_TARGET, -1);
}

static void texture_id_changed(GtkComboBox *id_box, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint target, id, levels, channels;
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
                           COLUMN_TEXTURE_ID_CHANNELS, &channels,
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
                    data->channels = channels;
                    data->flags = 0;
                    if (l == 0 && i == 0)
                    {
                        data->flags |= TEXTURE_CALLBACK_FLAG_FIRST;
                        data->nlevels = levels;
                        data->nplanes = 6;
                        data->type = GLDB_GUI_IMAGE_TYPE_CUBE;
                    }
                    if (l == levels - 1 && i == 5) data->flags |= TEXTURE_CALLBACK_FLAG_LAST;
                    set_response_handler(context, seq, response_callback_texture, data);
                    gldb_send_data_texture(seq++, id, target, data->face, l,
                                           gldb_channel_get_texture_token(channels),
                                           GL_FLOAT);
                }
            }
            else
#endif
            {
                data = (texture_callback_data *) bugle_malloc(sizeof(texture_callback_data));
                data->target = target;
                data->face = target;
                data->level = l;
                data->channels = channels;
                data->flags = 0;
                if (l == 0)
                {
                    data->flags |= TEXTURE_CALLBACK_FLAG_FIRST;
                    data->nlevels = levels;
                    data->nplanes = 1;
                    data->type = GLDB_GUI_IMAGE_TYPE_2D;
#ifdef GL_EXT_texture_3D
                    if (target == GL_TEXTURE_3D_EXT)
                    {
                        data->type = GLDB_GUI_IMAGE_TYPE_3D;
                        data->nplanes = 0;
                    }
#endif
                }
                if (l == levels - 1) data->flags |= TEXTURE_CALLBACK_FLAG_LAST;
                set_response_handler(context, seq, response_callback_texture, data);
                gldb_send_data_texture(seq++, id, target, target, l,
                                       gldb_channel_get_texture_token(channels),
                                       GL_FLOAT);
            }
        }
    }
}

static GtkWidget *build_texture_page_id(GldbWindow *context)
{
    GtkListStore *store;
    GtkWidget *id;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(5,
                               G_TYPE_UINT,  /* ID */
                               G_TYPE_UINT,  /* target */
                               G_TYPE_UINT,  /* level */
                               G_TYPE_UINT,  /* channels */
                               G_TYPE_STRING);
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

static GtkWidget *build_texture_page_combos(GldbWindow *context)
{
    GtkWidget *combos;
    GtkWidget *label, *id, *level, *zoom, *min, *mag;

    combos = gtk_table_new(3, 2, FALSE);

    label = gtk_label_new(_("Texture"));
    id = build_texture_page_id(context);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 0, 1, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), id, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Level"));
    level = gldb_gui_image_viewer_level_new(context->texture.viewer);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 1, 2, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), level, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Zoom"));
    zoom = context->texture.viewer->zoom;
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 2, 3, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), zoom, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Mag filter"));
    mag = gldb_gui_image_viewer_filter_new(context->texture.viewer, true);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 3, 4, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), mag, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Min filter"));
    min = gldb_gui_image_viewer_filter_new(context->texture.viewer, false);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 4, 5, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), min, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    return combos;
}

static GtkWidget *build_texture_page_toolbar(GldbWindow *context)
{
    GtkWidget *toolbar;
    GtkToolItem *item;

    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

    item = context->texture.viewer->copy;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    item = context->texture.viewer->zoom_in;
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = context->texture.viewer->zoom_out;
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = context->texture.viewer->zoom_100;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = context->texture.viewer->zoom_fit;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    return toolbar;
}

static void build_texture_page(GldbWindow *context)
{
    GtkWidget *label, *toolbar;
    GtkWidget *vbox, *hbox, *combos, *scrolled;
    gint page;

    context->texture.viewer = gldb_gui_image_viewer_new(GTK_STATUSBAR(context->statusbar),
                                                        context->statusbar_context_id);
    combos = build_texture_page_combos(context);
    toolbar = build_texture_page_toolbar(context);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
                                          context->texture.viewer->alignment);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), combos, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    label = gtk_label_new(_("Textures"));
    page = gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook),
                                    vbox, label);
    context->texture.dirty = false;
    context->texture.page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(context->notebook), page);
}


static void update_framebuffer_ids(GldbWindow *context)
{
    GValue old[2];
    GtkTreeIter iter;
    const gldb_state *root, *framebuffer, *sz;
    GtkTreeModel *model;
    char *name;
    int c;

    combo_box_get_old(GTK_COMBO_BOX(context->framebuffer.id), old,
                      COLUMN_FRAMEBUFFER_ID_ID, COLUMN_FRAMEBUFFER_ID_TARGET, -1);
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->framebuffer.id));
    gtk_list_store_clear(GTK_LIST_STORE(model));
    root = gldb_state_update();

#ifdef GL_EXT_framebuffer_object
    if ((framebuffer = state_find_child_enum(root, GL_FRAMEBUFFER_EXT)) != NULL)
    {
        bugle_list_node *pfbo;
        const gldb_state *fbo;

        for (pfbo = bugle_list_head(&framebuffer->children); pfbo != NULL; pfbo = bugle_list_next(pfbo))
        {
            fbo = (const gldb_state *) bugle_list_data(pfbo);

            if (fbo->numeric_name == 0)
                bugle_asprintf(&name, _("Default"));
            else
                bugle_asprintf(&name, "%u", (unsigned int) fbo->numeric_name);
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COLUMN_FRAMEBUFFER_ID_ID, fbo->numeric_name,
                               COLUMN_FRAMEBUFFER_ID_TARGET, GL_FRAMEBUFFER_EXT,
                               COLUMN_FRAMEBUFFER_ID_TEXT, name,
                               -1);
            free(name);
        }
    }
    else
#endif
    {
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COLUMN_FRAMEBUFFER_ID_ID, 0,
                           COLUMN_FRAMEBUFFER_ID_TARGET, 0,
                           COLUMN_FRAMEBUFFER_ID_TEXT, _("Default"),
                           -1);
    }

    combo_box_restore_old(GTK_COMBO_BOX(context->framebuffer.id), old,
                          COLUMN_FRAMEBUFFER_ID_ID, COLUMN_FRAMEBUFFER_ID_TARGET, -1);
}

static void framebuffer_id_changed(GtkWidget *widget, gpointer user_data)
{
    GldbWindow *context;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean stereo = false, doublebuffer = false;
    guint channels = 0, color_channels;
    guint id;
    GValue old_buffer;
    const gldb_state *root, *framebuffer, *fbo, *parameter;
    int i, attachments;
    char *name;

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
    {
        context = (GldbWindow *) user_data;
        root = gldb_state_update();

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->framebuffer.id));
        if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->framebuffer.id), &iter))
            return;
        gtk_tree_model_get(model, &iter,
                           COLUMN_FRAMEBUFFER_ID_ID, &id,
                           -1);

        combo_box_get_old(GTK_COMBO_BOX(context->framebuffer.buffer), &old_buffer,
                          COLUMN_FRAMEBUFFER_BUFFER_ID, -1);
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->framebuffer.buffer));
        gtk_list_store_clear(GTK_LIST_STORE(model));

        if (id != 0)
        {
#ifdef GL_EXT_framebuffer_object
            framebuffer = state_find_child_enum(root, GL_FRAMEBUFFER_EXT);
            g_assert(framebuffer != NULL);
            fbo = state_find_child_numeric(root, id);
            g_assert(fbo != NULL);

            for (i = 0; gldb_channel_table[i].channel; i++)
                if (gldb_channel_table[i].framebuffer_size_token
                    && (parameter = state_find_child_enum(fbo, gldb_channel_table[i].framebuffer_size_token)) != NULL
                    && strcmp(parameter->value, "0") != 0)
                {
                    channels |= gldb_channel_table[i].channel;
                }
            color_channels = gldb_channel_get_query_channels(channels & ~GLDB_CHANNEL_DEPTH_STENCIL);

            parameter = state_find_child_enum(fbo, GL_MAX_COLOR_ATTACHMENTS_EXT);
            g_assert(parameter != NULL);
            attachments = atoi(parameter->value);
            /* FIXME: check the attachments to determine whether anything is
             * attached.
             */
            for (i = 0; i < attachments; i++)
            {
                bugle_asprintf(&name, _("Color %d"), i);
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COLUMN_FRAMEBUFFER_BUFFER_ID, (guint) (GL_COLOR_ATTACHMENT0_EXT + i),
                                   COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, color_channels,
                                   COLUMN_FRAMEBUFFER_BUFFER_TEXT, name,
                                   -1);
                free(name);
            }

            if (channels & GLDB_CHANNEL_DEPTH)
            {
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COLUMN_FRAMEBUFFER_BUFFER_ID, (guint) GL_DEPTH_ATTACHMENT_EXT,
                                   COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, (guint) GLDB_CHANNEL_DEPTH,
                                   COLUMN_FRAMEBUFFER_BUFFER_TEXT, _("Depth"),
                                   -1);
            }
            if (channels & GLDB_CHANNEL_STENCIL)
            {
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COLUMN_FRAMEBUFFER_BUFFER_ID, (guint) GL_STENCIL_ATTACHMENT_EXT,
                                   COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, (guint) GLDB_CHANNEL_STENCIL,
                                   COLUMN_FRAMEBUFFER_BUFFER_TEXT, _("Stencil"),
                                   -1);
            }
#else
            g_assert_not_reached();
#endif
        }
        else
        {
            for (i = 0; gldb_channel_table[i].channel; i++)
                if (gldb_channel_table[i].framebuffer_size_token
                    && (parameter = state_find_child_enum(root, gldb_channel_table[i].framebuffer_size_token)) != NULL
                    && strcmp(parameter->value, "0") != 0)
                {
                    channels |= gldb_channel_table[i].channel;
                }
            color_channels = gldb_channel_get_query_channels(channels & ~GLDB_CHANNEL_DEPTH_STENCIL);

            if ((parameter = state_find_child_enum(root, GL_DOUBLEBUFFER)) != NULL
                && strcmp(parameter->value, "GL_TRUE") == 0)
                doublebuffer = true;
            if ((parameter = state_find_child_enum(root, GL_STEREO)) != NULL
                && strcmp(parameter->value, "GL_TRUE") == 0)
                stereo = true;

            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COLUMN_FRAMEBUFFER_BUFFER_ID, (guint) (stereo ? GL_FRONT_LEFT : GL_FRONT),
                               COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, color_channels,
                               COLUMN_FRAMEBUFFER_BUFFER_TEXT, stereo ? _("Front Left") : _("Front"),
                               -1);
            if (stereo)
            {
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COLUMN_FRAMEBUFFER_BUFFER_ID, (guint) GL_FRONT_RIGHT,
                                   COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, color_channels,
                                   COLUMN_FRAMEBUFFER_BUFFER_TEXT, _("Front Right"),
                                   -1);
            }
            if (doublebuffer)
            {
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COLUMN_FRAMEBUFFER_BUFFER_ID, (guint) (stereo ? GL_BACK_LEFT : GL_BACK),
                                   COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, color_channels,
                                   COLUMN_FRAMEBUFFER_BUFFER_TEXT, stereo ? _("Back Left") : _("Back"),
                                   -1);
                if (stereo)
                {
                    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                       COLUMN_FRAMEBUFFER_BUFFER_ID, (guint) GL_BACK_RIGHT,
                                       COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, color_channels,
                                       COLUMN_FRAMEBUFFER_BUFFER_TEXT, _("Back Right"),
                                       -1);
                }
            }
            if ((parameter = state_find_child_enum(root, GL_AUX_BUFFERS)) != NULL)
            {
                attachments = atoi(parameter->value);
                for (i = 0; i < attachments; i++)
                {
                    bugle_asprintf(&name, _("GL_AUX%d"), i);
                    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                       COLUMN_FRAMEBUFFER_BUFFER_ID, (guint) (GL_AUX0 + i),
                                       COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, color_channels,
                                       COLUMN_FRAMEBUFFER_BUFFER_TEXT, name,
                                       -1);
                    free(name);
                }
            }

            /* For depth and stencil, the buffer does not matter, but we
             * need to use a unique identifier for combo_box_restore_old to
             * work as intended. We filter out these illegal values later.
             */
            channels &= GLDB_CHANNEL_DEPTH_STENCIL;
            for (i = 0; gldb_channel_table[i].channel; i++)
                if ((gldb_channel_table[i].channel & channels) != 0
                    && gldb_channel_table[i].framebuffer_size_token /* avoid depth-stencil */
                    && gldb_channel_table[i].text)
                {
                    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                       COLUMN_FRAMEBUFFER_BUFFER_ID, gldb_channel_table[i].channel,
                                       COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, gldb_channel_table[i].channel,
                                       COLUMN_FRAMEBUFFER_BUFFER_TEXT, gldb_channel_table[i].text,
                                       -1);
                }
        }
        combo_box_restore_old(GTK_COMBO_BOX(context->framebuffer.buffer), &old_buffer,
                              COLUMN_FRAMEBUFFER_BUFFER_ID, -1);
    }
}

static void framebuffer_buffer_changed(GtkWidget *widget, gpointer user_data)
{
    GldbWindow *context;
    framebuffer_callback_data *data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    guint id, target, buffer, channel;

    context = (GldbWindow *) user_data;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->framebuffer.id));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->framebuffer.id), &iter))
        return;
    gtk_tree_model_get(model, &iter,
                       COLUMN_FRAMEBUFFER_ID_ID, &id,
                       COLUMN_FRAMEBUFFER_ID_TARGET, &target,
                       -1);
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->framebuffer.buffer));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->framebuffer.buffer), &iter))
        return;
    gtk_tree_model_get(model, &iter,
                       COLUMN_FRAMEBUFFER_BUFFER_ID, &buffer,
                       COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, &channel,
                       -1);

    if (!id && (channel & GLDB_CHANNEL_DEPTH_STENCIL))
    {
        /* We used an illegal but unique value for id to assist
         * combo_box_restore_old. Put in GL_FRONT, which is always legal.
         * The debugger filter-set may eventually ignore the value.
         */
        buffer = GL_FRONT;
    }

    data = (framebuffer_callback_data *) bugle_malloc(sizeof(framebuffer_callback_data));
    data->channels = gldb_channel_get_query_channels(channel);
    data->flags = 0;
    set_response_handler(context, seq, response_callback_framebuffer, data);
    gldb_send_data_framebuffer(seq++, id, target, buffer,
                               gldb_channel_get_framebuffer_token(data->channels),
                               GL_FLOAT);
}

static GtkWidget *build_framebuffer_page_id(GldbWindow *context)
{
    GtkListStore *store;
    GtkWidget *id;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(3,
                               G_TYPE_UINT,  /* ID */
                               G_TYPE_UINT,  /* target */
                               G_TYPE_STRING);

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
                                         COLUMN_FRAMEBUFFER_ID_ID,
                                         GTK_SORT_ASCENDING);
    id = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_widget_set_size_request(id, 80, -1);

    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(id), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(id), cell,
                                   "text", COLUMN_FRAMEBUFFER_ID_TEXT, NULL);
    g_signal_connect(G_OBJECT(id), "changed",
                     G_CALLBACK(framebuffer_id_changed), context);

    context->framebuffer.id = id;
    g_object_unref(G_OBJECT(store));
    return id;
}

static GtkWidget *build_framebuffer_page_buffer(GldbWindow *context)
{
    GtkListStore *store;
    GtkTreeIter iter;
    GtkWidget *buffer;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(3,
                               G_TYPE_UINT,    /* id */
                               G_TYPE_UINT,    /* channels */
                               G_TYPE_STRING); /* text */

    context->framebuffer.buffer = buffer = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(buffer), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(buffer), cell,
                                   "text", COLUMN_FRAMEBUFFER_BUFFER_TEXT, NULL);
    g_signal_connect(G_OBJECT(buffer), "changed",
                     G_CALLBACK(framebuffer_buffer_changed), context);
    g_object_unref(G_OBJECT(store));
    return buffer;
}

static GtkWidget *build_framebuffer_page_combos(GldbWindow *context)
{
    GtkWidget *combos;
    GtkWidget *label, *id, *buffer, *zoom;

    combos = gtk_table_new(4, 2, FALSE);

    label = gtk_label_new(_("Framebuffer"));
    id = build_framebuffer_page_id(context);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 0, 1, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), id, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Buffer"));
    buffer = build_framebuffer_page_buffer(context);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 1, 2, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), buffer, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Zoom"));
    zoom = context->framebuffer.viewer->zoom;
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 3, 4, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), zoom, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    return combos;
}

static GtkWidget *build_framebuffer_page_toolbar(GldbWindow *context)
{
    GtkWidget *toolbar;
    GtkToolItem *item;

    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

    item = context->framebuffer.viewer->copy;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    item = context->framebuffer.viewer->zoom_in;
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = context->framebuffer.viewer->zoom_out;
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = context->framebuffer.viewer->zoom_100;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = context->framebuffer.viewer->zoom_fit;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    return toolbar;
}

static void build_framebuffer_page(GldbWindow *context)
{
    GtkWidget *hbox, *vbox, *label, *combos, *scrolled, *toolbar;
    gint page;

    context->framebuffer.viewer = gldb_gui_image_viewer_new(GTK_STATUSBAR(context->statusbar),
                                                            context->statusbar_context_id);
    combos = build_framebuffer_page_combos(context);
    toolbar = build_framebuffer_page_toolbar(context);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
                                          context->framebuffer.viewer->alignment);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), combos, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    label = gtk_label_new(_("Framebuffers"));
    page = gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook),
                                    vbox, label);

    context->framebuffer.dirty = false;
    context->framebuffer.page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(context->notebook), page);
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

static gboolean shader_source_expose(GtkWidget *widget,
                                     GdkEventExpose *event,
                                     gpointer user_data)
{
    PangoLayout *layout;
    gint lines, firsty, lasty, dummy;
    GtkTextBuffer *buffer;
    GtkTextIter line;

    /* Check that this is an expose on the line number area */
    if (gtk_text_view_get_window_type(GTK_TEXT_VIEW(widget), event->window)
        != GTK_TEXT_WINDOW_LEFT)
        return FALSE;

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
    lines = gtk_text_buffer_get_line_count(buffer);
    layout = gtk_widget_create_pango_layout(widget, NULL);

    /* Find the first line that is in the expose area */
    firsty = event->area.y;
    lasty = event->area.y + event->area.height;
    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
                                          GTK_TEXT_WINDOW_LEFT,
                                          0, firsty,
                                          &dummy, &firsty);
    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
                                          GTK_TEXT_WINDOW_LEFT,
                                          0, lasty,
                                          &dummy, &lasty);
    gtk_text_view_get_line_at_y(GTK_TEXT_VIEW(widget), &line, firsty, NULL);

    /* Loop over the visible lines, displaying the line number */
    while (!gtk_text_iter_is_end(&line))
    {
        int y, height;
        int text_width, text_height;
        int winx, winy;
        char *no;

        gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(widget), &line,
                                      &y, &height);
        gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(widget),
                                              GTK_TEXT_WINDOW_LEFT,
                                              0, y, &winx, &winy);
        bugle_asprintf(&no, "%d", (int) gtk_text_iter_get_line(&line) + 1);
        pango_layout_set_text(layout, no, -1);
        free(no);
        pango_layout_get_pixel_size(layout, &text_width, &text_height);
        gtk_paint_layout(gtk_widget_get_style(widget), event->window,
                         GTK_STATE_NORMAL, FALSE, NULL,
                         widget, NULL, 35 - text_width, winy, layout);
        if (y + height >= lasty) break;
        gtk_text_iter_forward_line(&line);
    }
    g_object_unref(layout);
    return FALSE;
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
    gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(source),
                                         GTK_TEXT_WINDOW_LEFT,
                                         40);
    g_signal_connect(G_OBJECT(source), "expose-event", G_CALLBACK(shader_source_expose), NULL);

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
"      <menuitem action='Step' />"
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
    { "Step", NULL, "_Step", NULL, NULL, G_CALLBACK(step_action) },
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
    /* Statusbar must be built early, because it is passed to image viewers */
    context->statusbar = gtk_statusbar_new();
    context->statusbar_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(context->statusbar), _("Program status"));
    gtk_statusbar_push(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id, _("Not running"));

    build_state_page(context);
#if HAVE_GTKGLEXT
    build_texture_page(context);
    build_framebuffer_page(context);
#endif
#ifdef GLDB_GUI_SHADER
    build_shader_page(context);
#endif
    build_backtrace_page(context);
    gtk_box_pack_start(GTK_BOX(vbox), context->notebook, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(context->notebook), "switch-page",
                     G_CALLBACK(notebook_switch_page), context);

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

int main(int argc, char **argv)
{
    GldbWindow context;

    gtk_init(&argc, &argv);
#if HAVE_GTKGLEXT
    gtk_gl_init(&argc, &argv);
    gldb_gui_image_initialise();
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
    gldb_gui_image_shutdown();
#endif
    g_object_unref(G_OBJECT(context.breakpoints_store));
    return 0;
}
