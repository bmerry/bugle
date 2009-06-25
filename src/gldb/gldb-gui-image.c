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

/* Functions in gldb-gui that deal with the display of images such as
 * textures and framebuffers.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#if HAVE_GTKGLEXT

/* FIXME: Not sure if this is the correct definition of GETTEXT_PACKAGE... */
#ifndef GETTEXT_PACKAGE
# define GETTEXT_PACKAGE "bugle00"
#endif
#include <GL/glew.h>
#include <gtk/gtkgl.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <math.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include "gldb/gldb-common.h"
#include "gldb/gldb-channels.h"
#include "gldb/gldb-gui.h"
#include "gldb/gldb-gui-image.h"

enum
{
    COLUMN_IMAGE_ZOOM_VALUE,
    COLUMN_IMAGE_ZOOM_TEXT,
    COLUMN_IMAGE_ZOOM_SENSITIVE
};

enum
{
    COLUMN_IMAGE_LEVEL_VALUE,
    COLUMN_IMAGE_LEVEL_TEXT
};

enum
{
    COLUMN_IMAGE_FACE_VALUE,
    COLUMN_IMAGE_FACE_TEXT
};

enum
{
    COLUMN_IMAGE_FILTER_VALUE,
    COLUMN_IMAGE_FILTER_TEXT,
    COLUMN_IMAGE_FILTER_NON_MIP
};

static GtkTreeModel *mag_filter_model, *min_filter_model, *face_model;

static void image_draw_realize(GtkWidget *widget, gpointer user_data)
{
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;
    GldbGuiImageViewer *viewer;

    viewer = (GldbGuiImageViewer *) user_data;
    glcontext = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) return;

    glewInit();
    glViewport(0, 0, widget->allocation.width, widget->allocation.height);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    /* NVIDIA do not expose GL_SGIS_texture_edge_clamp (instead they
     * expose the unregistered GL_EXT_texture_edge_clamp), so we use
     * the OpenGL version as the test instead.
     */
    if (strcmp((char *) glGetString(GL_VERSION), "1.2") >= 0)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        if (gdk_gl_query_gl_extension("GL_EXT_texture3D"))
        {
            glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_WRAP_R_EXT, GL_CLAMP);
        }
    }
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewer->max_viewport_dims);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    gdk_gl_drawable_gl_end(gldrawable);

    gdk_window_set_events(widget->window,
                          gdk_window_get_events(widget->window)
                          | GDK_POINTER_MOTION_MASK
                          | GDK_ENTER_NOTIFY_MASK
                          | GDK_LEAVE_NOTIFY_MASK);
}

static gboolean image_draw_configure(GtkWidget *widget,
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

static void image_draw_expose_2d(GldbGuiImageViewer *viewer)
{
    GLfloat s, t;
    GLfloat tex_coords[8] =
    {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };
    static const GLint vertex_coords[8] =
    {
        -1, -1,
        +1, -1,
        +1, +1,
        -1, +1
    };

    s = viewer->current->s;
    t = viewer->current->t;
    tex_coords[2] = tex_coords[4] = s;
    tex_coords[5] = tex_coords[7] = t;

    glVertexPointer(2, GL_INT, 0, vertex_coords);
    glTexCoordPointer(2, GL_FLOAT, 0, tex_coords);
    glDrawArrays(GL_QUADS, 0, 4);
}

static void image_draw_expose_3d(GldbGuiImageViewer *viewer)
{
    int i;
    int level;
    int nplanes;
    GLfloat s, t, r;
    GLfloat tex_coords[12] =
    {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };
    static const GLint vertex_coords[8] =
    {
        -1, -1,
        +1, -1,
        +1, +1,
        -1, +1
    };

    level = MAX(viewer->current_level, 0);
    nplanes = viewer->current->levels[level].nplanes;
    s = viewer->current->s;
    t = viewer->current->t;
    r = viewer->current->r * (CLAMP(viewer->current_plane, 0, nplanes - 1) + 0.5f) / nplanes;
    tex_coords[3] = tex_coords[6] = s;
    tex_coords[7] = tex_coords[10] = t;
    for (i = 2; i < 12; i += 3)
        tex_coords[i] = r;

    glVertexPointer(2, GL_INT, 0, vertex_coords);
    glTexCoordPointer(3, GL_FLOAT, 0, tex_coords);
    glDrawArrays(GL_QUADS, 0, 4);
}

static void image_draw_expose_cube_map(GldbGuiImageViewer *viewer)
{
    /* Note: this is intentionally inverted to make the texture appear the
     * right way round i.e. in the layout passed to glTexImage2D when
     * specifying the cube-map.
     */
    static const GLint square_vertices[8] =
    {
        +1, -1,
        -1, -1,
        -1, +1,
        +1, +1
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
        6, 7, 5, 4,
        3, 2, 0, 1,
        6, 2, 3, 7,
        5, 1, 0, 4,
        7, 3, 1, 5,
        2, 6, 4, 0
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

    if (gdk_gl_query_gl_extension("GL_ARB_texture_cube_map"))
    {
        if (viewer->current_plane < 0)
        {
            glEnable(GL_CULL_FACE);
            glVertexPointer(3, GL_INT, 0, cube_vertices);
            glTexCoordPointer(3, GL_INT, 0, cube_vertices);

            /* Force all Z coordinates to 0 to avoid face clipping, and
             * scale down to fit in left half of window */
            glTranslatef(-0.5f, 0.0f, 0.0f);
            glScalef(0.25f, 0.5f, 0.0f);
            glMultMatrixd(cube_matrix1);

            glDrawElements(GL_QUADS, 24, GL_UNSIGNED_BYTE, cube_indices);

            /* Draw second view */
            glLoadIdentity();
            glTranslatef(0.5f, 0.0f, 0.0f);
            glScalef(0.25f, 0.5, 0.5f);
            glMultMatrixd(cube_matrix2);
            glDrawElements(GL_QUADS, 24, GL_UNSIGNED_BYTE, cube_indices);

            /* restore state */
            glLoadIdentity();
            glDisable(GL_CULL_FACE);
        }
        else
        {
            int i, index;

            glBegin(GL_QUADS);
            for (i = 0; i < 4; i++)
            {
                index = cube_indices[4 * viewer->current_plane + i];
                glTexCoord3iv(cube_vertices + 3 * index);
                glVertex2iv(square_vertices + 2 * i);
            }
            glEnd();
        }
    }
    else
        g_warning("No cube-map support in OpenGL!");
}

static gboolean image_draw_expose(GtkWidget *widget,
                                  GdkEventExpose *event,
                                  gpointer user_data)
{
    GldbGuiImageViewer *viewer;
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;

    g_assert(gtk_widget_is_gl_capable(widget));
    glcontext = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
    {
        g_warning("Not able to update image");
        return FALSE;
    }
    glClear(GL_COLOR_BUFFER_BIT);

    viewer = (GldbGuiImageViewer *) user_data;
    if (viewer->current)
    {
        glEnable(viewer->current->texture_target);
        if (strcmp((char *) glGetString(GL_VERSION), "1.2") >= 0)
        {
            if (viewer->current_level >= 0)
            {
                glTexParameteri(viewer->current->texture_target, GL_TEXTURE_BASE_LEVEL, viewer->current_level);
                glTexParameteri(viewer->current->texture_target, GL_TEXTURE_MAX_LEVEL, viewer->current_level);
            }
            else
            {
                glTexParameteri(viewer->current->texture_target, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(viewer->current->texture_target, GL_TEXTURE_MAX_LEVEL, 1000);
            }
        }
        glTexParameteri(viewer->current->texture_target, GL_TEXTURE_MAG_FILTER, viewer->texture_mag_filter);
        glTexParameteri(viewer->current->texture_target, GL_TEXTURE_MIN_FILTER, viewer->texture_min_filter);

        if (viewer->current->type == GLDB_GUI_IMAGE_TYPE_2D)
            image_draw_expose_2d(viewer);
        else if (viewer->current->type == GLDB_GUI_IMAGE_TYPE_3D)
            image_draw_expose_3d(viewer);
        else if (viewer->current->type == GLDB_GUI_IMAGE_TYPE_CUBE_MAP)
            image_draw_expose_cube_map(viewer);
        else
            g_assert_not_reached();
        glDisable(viewer->current->texture_target);
    }
    /* A finish should absolutely not be needed here, but apparently there
     * are some bugs somewhere on my machine that cause repaints to have
     * no effect when switching from a pane without 3D (e.g. state) to
     * a pane with an image viewer, IF a tearoff menu is currently torn off.
     */
    glFinish();
    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);
    return FALSE;
}

static void image_draw_motion_clear_status(GldbGuiImageViewer *viewer)
{
    if (viewer->pixel_status_id != 0)
    {
        gtk_statusbar_remove(viewer->statusbar,
                             viewer->statusbar_context_id,
                             viewer->pixel_status_id);
        viewer->pixel_status_id = 0;
    }
}

static void image_draw_motion_update(GldbGuiImageViewer *viewer,
                                     GtkWidget *draw,
                                     gdouble x, gdouble y)
{
    int width, height;
    int level;
    int u, v;
    GLfloat *pixel;
    char *msg, *tmp_msg;
    int nchannels, p;
    uint32_t channels, channel;
    const GldbGuiImagePlane *plane;

    image_draw_motion_clear_status(viewer);
    /* FIXME: handle cube-map textures */
    if (!viewer->current)
        return;
    if (viewer->current_plane < 0)
        return;  /* cube map - currently no raytracing to recover the pixel */

    level = MAX(viewer->current_level, 0);
    plane = &viewer->current->levels[level].planes[viewer->current_plane];
    width = plane->width;
    height = plane->height;
    x = (x + 0.5) / draw->allocation.width * width;
    y = (1.0 - (y + 0.5) / draw->allocation.height) * height;
    u = CLAMP((int) x, 0, width - 1);
    v = CLAMP((int) y, 0, height - 1);
    nchannels = gldb_channel_count(plane->channels);
    pixel = plane->pixels + nchannels * (v * width + u);
    msg = bugle_asprintf("u: %d v: %d ", u, v);

    channels = plane->channels;
    for (p = 0; channels; channels &= ~channel, p++)
    {
        channel = channels & ~(channels - 1);
        const char *abbr = gldb_channel_get_abbr(channel);
        tmp_msg = msg;
        msg = bugle_asprintf(" %s %s: %f", tmp_msg, abbr ? abbr : "?", gldb_gui_image_plane_get_pixel(plane, u, v, p));
        bugle_free(tmp_msg);
    }
    viewer->pixel_status_id = gtk_statusbar_push(viewer->statusbar,
                                                 viewer->statusbar_context_id,
                                                 msg);
    bugle_free(msg);
}

static gboolean image_draw_motion(GtkWidget *widget,
                                  GdkEventMotion *event,
                                  gpointer user_data)
{
    image_draw_motion_update((GldbGuiImageViewer *) user_data,
                             widget, event->x, event->y);
    return FALSE;
}

static gboolean image_draw_enter(GtkWidget *widget,
                                 GdkEventCrossing *event,
                                 gpointer user_data)
{
    image_draw_motion_update((GldbGuiImageViewer *) user_data,
                             widget, event->x, event->y);
    return FALSE;
}

static gboolean image_draw_leave(GtkWidget *widget,
                                 GdkEventCrossing *event,
                                 gpointer user_data)
{
    GldbGuiImageViewer *viewer;

    viewer = (GldbGuiImageViewer *) user_data;
    if (viewer->pixel_status_id != 0)
    {
        gtk_statusbar_remove(viewer->statusbar,
                             viewer->statusbar_context_id,
                             viewer->pixel_status_id);
        viewer->pixel_status_id = 0;
    }
    return FALSE;
}

static void gldb_gui_image_viewer_new_draw(GldbGuiImageViewer *viewer)
{
    GdkGLConfig *glconfig;
    GtkWidget *aspect, *alignment, *draw;
    static const int attrib_list[] =
    {
        GDK_GL_USE_GL, TRUE,
        GDK_GL_RGBA, TRUE,
        GDK_GL_DOUBLEBUFFER, TRUE,
        GDK_GL_ATTRIB_LIST_NONE
    };

    glconfig = gdk_gl_config_new(attrib_list);
    g_assert(glconfig != NULL);
    draw = gtk_drawing_area_new();
    gtk_widget_set_size_request(draw, 1, 1);
    gtk_widget_set_gl_capability(draw, glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);
    g_signal_connect_after(G_OBJECT(draw), "realize",
                           G_CALLBACK(image_draw_realize), viewer);
    g_signal_connect(G_OBJECT(draw), "configure-event",
                     G_CALLBACK(image_draw_configure), viewer);
    g_signal_connect(G_OBJECT(draw), "expose-event",
                     G_CALLBACK(image_draw_expose), viewer);
    g_signal_connect(G_OBJECT(draw), "motion-notify-event",
                     G_CALLBACK(image_draw_motion), viewer);
    g_signal_connect(G_OBJECT(draw), "enter-notify-event",
                     G_CALLBACK(image_draw_enter), viewer);
    g_signal_connect(G_OBJECT(draw), "leave-notify-event",
                     G_CALLBACK(image_draw_leave), viewer);

    aspect = gtk_aspect_frame_new(NULL, 0.5, 0.5, 1.0, TRUE);
    gtk_frame_set_shadow_type(GTK_FRAME(aspect), GTK_SHADOW_NONE);
    alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_container_add(GTK_CONTAINER(aspect), draw);
    gtk_container_add(GTK_CONTAINER(alignment), aspect);

    viewer->alignment = alignment;
    viewer->draw = draw;
}

#if HAVE_GTK2_6
static gboolean zoom_row_separator(GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   gpointer data)
{
    gdouble value;

    gtk_tree_model_get(model, iter, COLUMN_IMAGE_ZOOM_VALUE, &value, -1);
    return value == -2.0;
}
#endif

static void resize_image_draw(GldbGuiImageViewer *viewer)
{
    GtkWidget *aspect, *alignment, *draw;
    GtkTreeModel *zoom_model;
    GtkTreeIter iter;
    gdouble zoom;
    int width, height;
    int level;
    const GldbGuiImagePlane *plane;

    if (!viewer->current)
        return;

    draw = viewer->draw;
    aspect = gtk_widget_get_parent(draw);
    alignment = viewer->alignment;

    level = MAX(viewer->current_level, 0);
    plane = &viewer->current->levels[level].planes[MAX(viewer->current_plane, 0)];
    width = MAX(plane->width, 1);
    height = MAX(plane->height, 1);
    if (viewer->current->type == GLDB_GUI_IMAGE_TYPE_CUBE_MAP
        && viewer->current_plane == -1)
        width *= 2; /* allow for two views of the cube */

    zoom_model = gtk_combo_box_get_model(GTK_COMBO_BOX(viewer->zoom));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(viewer->zoom),
                                      &iter))
    {
        gtk_tree_model_get(zoom_model, &iter,
                           COLUMN_IMAGE_ZOOM_VALUE, &zoom, -1);
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

static void image_zoom_changed(GtkWidget *zoom, gpointer user_data)
{
    GldbGuiImageViewer *viewer;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter, next;
    gboolean sensitive_in = FALSE;
    gboolean sensitive_out = FALSE;
    gboolean sensitive_100 = FALSE;
    gboolean sensitive;
    gboolean more;
    gdouble value;

    viewer = (GldbGuiImageViewer *) user_data;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(zoom));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(zoom), &iter))
    {
        next = iter;
        if (gtk_tree_model_iter_next(model, &next))
            gtk_tree_model_get(model, &next, COLUMN_IMAGE_ZOOM_SENSITIVE, &sensitive_in, -1);
        path = gtk_tree_model_get_path(model, &iter);
        if (gtk_tree_path_prev(path))
        {
            gtk_tree_model_get_iter(model, &iter, path);
            /* Check that we haven't hit the top */
            gtk_tree_model_get(model, &iter,
                               COLUMN_IMAGE_ZOOM_SENSITIVE, &sensitive_out,
                               COLUMN_IMAGE_ZOOM_VALUE, &value,
                               -1);
            if (value < 0.0) sensitive_out = FALSE;
        }
        gtk_tree_path_free(path);
    }

    more = gtk_tree_model_get_iter_first(model, &iter);
    while (more)
    {
        gtk_tree_model_get(model, &iter,
                           COLUMN_IMAGE_ZOOM_VALUE, &value,
                           COLUMN_IMAGE_ZOOM_SENSITIVE, &sensitive,
                           -1);
        if (value == 1.0)
        {
            sensitive_100 = sensitive;
            break;
        }
        more = gtk_tree_model_iter_next(model, &iter);
    }
    gtk_widget_set_sensitive(GTK_WIDGET(viewer->zoom_in), sensitive_in);
    gtk_widget_set_sensitive(GTK_WIDGET(viewer->zoom_out), sensitive_out);
    gtk_widget_set_sensitive(GTK_WIDGET(viewer->zoom_100), sensitive_100);

    resize_image_draw(viewer);
}

static void gldb_gui_image_viewer_new_zoom_combo(GldbGuiImageViewer *viewer)
{
    GtkListStore *store;
    GtkTreeIter iter;
    GtkWidget *zoom;
    GtkCellRenderer *cell;
    int i;

    store = gtk_list_store_new(3, G_TYPE_DOUBLE, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_IMAGE_ZOOM_VALUE, -1.0,
                       COLUMN_IMAGE_ZOOM_TEXT, _("Fit"),
                       COLUMN_IMAGE_ZOOM_SENSITIVE, TRUE,
                       -1);

#if HAVE_GTK2_6
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_IMAGE_ZOOM_VALUE, -2.0, /* -2.0 is magic separator value - see above function */
                       COLUMN_IMAGE_ZOOM_TEXT, "Separator",
                       COLUMN_IMAGE_ZOOM_SENSITIVE, FALSE,
                       -1);
#endif

    for (i = 5; i >= 0; i--)
    {
        gchar *caption;

        caption = bugle_asprintf("1:%d", (1 << i));
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_IMAGE_ZOOM_VALUE, (gdouble) 1.0 / (1 << i),
                           COLUMN_IMAGE_ZOOM_TEXT, caption,
                           COLUMN_IMAGE_ZOOM_SENSITIVE, FALSE,
                           -1);
        bugle_free(caption);
    }
    for (i = 1; i <= 5; i++)
    {
        gchar *caption;

        caption = bugle_asprintf("%d:1", (1 << i));
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_IMAGE_ZOOM_VALUE, (gdouble) (1 << i),
                           COLUMN_IMAGE_ZOOM_TEXT, caption,
                           COLUMN_IMAGE_ZOOM_SENSITIVE, FALSE,
                           -1);
        bugle_free(caption);
    }

    viewer->zoom = zoom = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(zoom), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(zoom), cell,
                                   "text", COLUMN_IMAGE_ZOOM_TEXT,
                                   "sensitive", COLUMN_IMAGE_ZOOM_SENSITIVE,
                                   NULL);
#if HAVE_GTK2_6
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(zoom),
                                         zoom_row_separator,
                                         NULL, NULL);
#endif
    gtk_combo_box_set_active(GTK_COMBO_BOX(zoom), 0);
    g_signal_connect(G_OBJECT(zoom), "changed",
                     G_CALLBACK(image_zoom_changed), viewer);
    g_object_unref(G_OBJECT(store));
}

#define FLOAT_TO_UBYTE(x) ((guchar) CLAMP(floor((x) * 255.0), 0, 255))
#define UBYTE_TO_FLOAT(x) ((x) / 255.0f)

static void free_pixbuf_data(guchar *pixels, gpointer user_data)
{
    bugle_free(pixels);
}

GLfloat gldb_gui_image_plane_get_pixel(const GldbGuiImagePlane *plane, int x, int y, int c)
{
    int nin;
    size_t offset;

    nin = gldb_channel_count(plane->channels);
    offset = ((size_t) y * plane->width + x) * nin + c;
    switch (plane->type)
    {
    case GL_FLOAT:
        return ((const GLfloat *) plane->pixels)[offset];
    case GL_UNSIGNED_BYTE:
        return UBYTE_TO_FLOAT(((const GLubyte *) plane->pixels)[offset]);
    default:
        g_return_val_if_reached(0);
    }
}

static void image_copy_clicked(GtkWidget *button, gpointer user_data)
{
#if HAVE_GTK2_6
    GLint width, height, nin, nout;
    guint8 *pixels, *p;
    GdkPixbuf *pixbuf = NULL;
    GtkClipboard *clipboard;
    GldbGuiImageViewer *viewer;
    int y, x, c;
    int level;
    GldbGuiImagePlane *plane;

    /* FIXME: support cube-map and 3D, or disable button when appropriate */

    viewer = (GldbGuiImageViewer *) user_data;
    if (!viewer->current)
        return;
    if (viewer->current_plane < 0)
        return;

    level = MAX(viewer->current_level, 0);
    plane = &viewer->current->levels[level].planes[MAX(viewer->current_plane, 0)];
    width = plane->width;
    height = plane->height;
    nin = gldb_channel_count(plane->channels);
    nout = CLAMP(nin, 3, 4);
    pixels = BUGLE_NMALLOC(width * height * nout, guint8);
    p = pixels;
    for (y = height - 1; y >= 0; y--)
        for (x = 0; x < width; x++)
            for (c = 0; c < nout; c++)
                *p++ = FLOAT_TO_UBYTE(gldb_gui_image_plane_get_pixel(plane, x, y, c % nin));

    pixbuf = gdk_pixbuf_new_from_data(pixels, GDK_COLORSPACE_RGB, nout == 4, 8,
                                      width, height, width * nout,
                                      free_pixbuf_data, NULL);
    if (!pixbuf) return;

    clipboard = gtk_clipboard_get_for_display(gtk_widget_get_display(button),
                                              GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_image(clipboard, pixbuf);
    g_object_unref(pixbuf);
#else  /* HAVE_GTK2_6 */

    GtkWidget *dialog;
    GtkWidget *window;

    window = gtk_widget_get_toplevel(button);
    dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_CLOSE,
                                    _("Copy to clipboard requires GTK+ 2.6 or later. Upgrade GTK+ then recompile gldb-gui."));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
#endif /* !HAVE_GTK2_6 */
}

static void image_zoom_in_clicked(GtkToolButton *toolbutton,
                                  gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GldbGuiImageViewer *viewer;
    gboolean sensitive;

    viewer = (GldbGuiImageViewer *) user_data;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(viewer->zoom));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(viewer->zoom), &iter)
        && gtk_tree_model_iter_next(model, &iter))
    {
        gtk_tree_model_get(model, &iter, COLUMN_IMAGE_ZOOM_SENSITIVE, &sensitive, -1);
        if (sensitive)
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(viewer->zoom), &iter);
    }
}

static void image_zoom_out_clicked(GtkToolButton *toolbutton,
                                   gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreePath *path;
    GldbGuiImageViewer *viewer;
    gboolean sensitive;

    viewer = (GldbGuiImageViewer *) user_data;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(viewer->zoom));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(viewer->zoom), &iter))
    {
        /* There is no gtk_tree_model_iter_prev, so we have to go via tree paths. */
        path = gtk_tree_model_get_path(model, &iter);
        if (gtk_tree_path_prev(path))
        {
            gtk_tree_model_get_iter(model, &iter, path);
            gtk_tree_model_get(model, &iter, COLUMN_IMAGE_ZOOM_SENSITIVE, &sensitive, -1);
            if (sensitive)
                gtk_combo_box_set_active_iter(GTK_COMBO_BOX(viewer->zoom), &iter);
        }
        gtk_tree_path_free(path);
    }
}

static void image_zoom_100_clicked(GtkToolButton *toolbutton,
                                   gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GldbGuiImageViewer *viewer;
    gdouble zoom;
    gboolean sensitive, more;

    viewer = (GldbGuiImageViewer *) user_data;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(viewer->zoom));
    more = gtk_tree_model_get_iter_first(model, &iter);
    while (more)
    {
        gtk_tree_model_get(model, &iter,
                           COLUMN_IMAGE_ZOOM_VALUE, &zoom,
                           COLUMN_IMAGE_ZOOM_SENSITIVE, &sensitive,
                           -1);
        if (zoom == 1.0)
        {
            if (sensitive)
                gtk_combo_box_set_active_iter(GTK_COMBO_BOX(viewer->zoom), &iter);
            return;
        }
        more = gtk_tree_model_iter_next(model, &iter);
    }
}

static void image_zoom_fit_clicked(GtkToolButton *toolbutton,
                                   gpointer user_data)
{
    GldbGuiImageViewer *viewer;

    viewer = (GldbGuiImageViewer *) user_data;
    gtk_combo_box_set_active(GTK_COMBO_BOX(viewer->zoom), 0);
}

static void gldb_gui_image_viewer_new_toolbuttons(GldbGuiImageViewer *viewer)
{
    GtkToolItem *item;

    viewer->copy = item = gtk_tool_button_new_from_stock(GTK_STOCK_COPY);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(image_copy_clicked), viewer);

    viewer->zoom_in = item = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(image_zoom_in_clicked), viewer);
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);

    viewer->zoom_out = item = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(image_zoom_out_clicked), viewer);
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);

    viewer->zoom_100 = item = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_100);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(image_zoom_100_clicked), viewer);

    viewer->zoom_fit = item = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_FIT);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(image_zoom_fit_clicked), viewer);
}

static void image_level_changed(GtkWidget *widget, gpointer user_data)
{
    GldbGuiImageViewer *viewer;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint level;

    viewer = (GldbGuiImageViewer *) user_data;
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter))
        return;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
    gtk_tree_model_get(model, &iter,
                       COLUMN_IMAGE_LEVEL_VALUE, &level,
                       -1);
    viewer->current_level = level;
    gldb_gui_image_viewer_update_zoom(viewer);
    gtk_widget_queue_draw(viewer->draw);
}

#if HAVE_GTK2_6
static gboolean image_level_row_separator(GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          gpointer user_data)
{
    gint value;

    gtk_tree_model_get(model, iter, COLUMN_IMAGE_LEVEL_VALUE, &value, -1);
    return value == -2;
}
#endif

GtkWidget *gldb_gui_image_viewer_level_new(GldbGuiImageViewer *viewer)
{
    GtkListStore *store;
    GtkTreeIter iter;
    GtkWidget *level;
    GtkCellRenderer *cell;

    g_assert(viewer->level == NULL);

    store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_IMAGE_LEVEL_VALUE, -1,
                       COLUMN_IMAGE_LEVEL_TEXT, _("Auto"),
                       -1);
#if HAVE_GTK2_6
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_IMAGE_LEVEL_VALUE, -2, /* -2 is magic separator value */
                       COLUMN_IMAGE_LEVEL_TEXT, "Separator",
                       -1);
#endif

    level = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(level), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(level), cell,
                                   "text", COLUMN_IMAGE_LEVEL_TEXT, NULL);
#if HAVE_GTK2_6
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(level),
                                         image_level_row_separator,
                                         NULL, NULL);
#endif
    gtk_combo_box_set_active(GTK_COMBO_BOX(level), 0);
    g_signal_connect(G_OBJECT(level), "changed",
                     G_CALLBACK(image_level_changed), viewer);
    g_object_unref(G_OBJECT(store));
    return viewer->level = level;
}

static void image_face_changed(GtkWidget *widget, gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GldbGuiImageViewer *viewer;
    gint face;

    viewer = (GldbGuiImageViewer *) user_data;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter))
        return;

    gtk_tree_model_get(model, &iter, COLUMN_IMAGE_FACE_VALUE, &face, -1);
    if (face >= -1)
    {
        viewer->current_plane = face;
        gldb_gui_image_viewer_update_zoom(viewer);
        gtk_widget_queue_draw(viewer->draw);
    }
}

#if HAVE_GTK2_6
static gboolean image_face_row_separator(GtkTreeModel *model,
                                         GtkTreeIter *iter,
                                         gpointer user_data)
{
    gint value;

    gtk_tree_model_get(model, iter, COLUMN_IMAGE_FACE_VALUE, &value, -1);
    return value == -2;
}
#endif

GtkWidget *gldb_gui_image_viewer_face_new(GldbGuiImageViewer *viewer)
{
    GtkWidget *face;
    GtkCellRenderer *cell;

    g_assert(viewer->face == NULL);
    face = gtk_combo_box_new_with_model(face_model);
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(face), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(face), cell,
                                   "text", COLUMN_IMAGE_FACE_TEXT, NULL);
#if HAVE_GTK2_6
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(face),
                                         image_face_row_separator,
                                         NULL, NULL);
#endif
    gtk_combo_box_set_active(GTK_COMBO_BOX(face), 0);
    g_signal_connect(G_OBJECT(face), "changed",
                     G_CALLBACK(image_face_changed), viewer);
    gtk_widget_set_sensitive(face, FALSE);
    return viewer->face = face;
}

static void image_viewer_zoffset_changed(GtkSpinButton *zoffset,
                                         gpointer user_data)
{
    GldbGuiImageViewer *viewer;

    viewer = (GldbGuiImageViewer *) user_data;
    viewer->current_plane = gtk_spin_button_get_value_as_int(zoffset);
    gtk_widget_queue_draw(viewer->draw);
}

GtkWidget *gldb_gui_image_viewer_zoffset_new(GldbGuiImageViewer *viewer)
{
    GtkWidget *zoffset;

    g_assert(viewer->zoffset == NULL);
    zoffset = gtk_spin_button_new_with_range(0, 0, 1);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(zoffset), TRUE);
    g_signal_connect(G_OBJECT(zoffset), "value-changed",
                     G_CALLBACK(image_viewer_zoffset_changed), viewer);
    return viewer->zoffset = zoffset;
}

static void image_viewer_remap_toggled(GtkToggleButton *widget, gpointer user_data)
{
    GldbGuiImageViewer *viewer;
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;

    viewer = (GldbGuiImageViewer *) user_data;
    if (!viewer->current)
        return;
    glcontext = gtk_widget_get_gl_context(viewer->draw);
    gldrawable = gtk_widget_get_gl_drawable(viewer->draw);
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        return;
    gldb_gui_image_upload(viewer->current, gtk_toggle_button_get_active(widget));
    gdk_gl_drawable_gl_end(gldrawable);

    gtk_widget_queue_draw(viewer->draw);
}

GtkWidget *gldb_gui_image_viewer_remap_new(GldbGuiImageViewer *viewer)
{
    GtkWidget *remap;

    g_assert(viewer->remap == NULL);
    remap = gtk_check_button_new_with_label("Remap range");
    g_signal_connect(G_OBJECT(remap), "toggled",
                     G_CALLBACK(image_viewer_remap_toggled), viewer);
    return viewer->remap = remap;
}

static void image_filter_changed(GtkWidget *widget, gpointer user_data)
{
    GldbGuiImageViewer *viewer;
    GtkTreeModel *model;
    GtkTreeIter iter;
    guint value;

    viewer = (GldbGuiImageViewer *) user_data;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter))
        return;
    gtk_tree_model_get(model, &iter,
                       COLUMN_IMAGE_FILTER_VALUE, &value,
                       -1);
    if (widget == viewer->mag_filter)
        viewer->texture_mag_filter = value;
    else
        viewer->texture_min_filter = value;
    gtk_widget_queue_draw(viewer->draw);
}

GtkWidget *gldb_gui_image_viewer_filter_new(GldbGuiImageViewer *viewer, bugle_bool mag)
{
    GtkCellRenderer *cell;
    GtkWidget *filter;

    filter = gtk_combo_box_new_with_model(mag ? mag_filter_model : min_filter_model);
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(filter), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(filter), cell,
                                   "text", COLUMN_IMAGE_FILTER_TEXT, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter), 0);
    if (mag)
    {
        g_assert(viewer->mag_filter == NULL);
        viewer->mag_filter = filter;
    }
    else
    {
        g_assert(viewer->min_filter == NULL);
        viewer->min_filter = filter;
        viewer->min_filter_renderer = cell;
    }
    g_signal_connect(G_OBJECT(filter), "changed",
                     G_CALLBACK(image_filter_changed), viewer);
    return filter;
}

GldbGuiImageViewer *gldb_gui_image_viewer_new(GtkStatusbar *statusbar,
                                              guint statusbar_context_id)
{
    GldbGuiImageViewer *viewer;

    viewer = BUGLE_ZALLOC(GldbGuiImageViewer);
    viewer->statusbar = statusbar;
    viewer->statusbar_context_id = statusbar_context_id;
    viewer->current_level = -1;
    viewer->current_plane = 0;
    viewer->texture_mag_filter = GL_NEAREST;
    viewer->texture_min_filter = GL_NEAREST;
    gldb_gui_image_viewer_new_draw(viewer);
    gldb_gui_image_viewer_new_zoom_combo(viewer);
    gldb_gui_image_viewer_new_toolbuttons(viewer);

    return viewer;
}

void gldb_gui_image_viewer_update_levels(GldbGuiImageViewer *viewer)
{
    GtkTreeModel *model;
    GtkTreeIter iter, new_iter;
    gint levels, i;
    gint old = -1;

    if (!viewer->current || !viewer->level)
        return;
    levels = viewer->current->nlevels;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(viewer->level));
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(viewer->level), &iter))
        gtk_tree_model_get(model, &iter, COLUMN_IMAGE_LEVEL_VALUE, &old, -1);
    gtk_list_store_clear(GTK_LIST_STORE(model));
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COLUMN_IMAGE_LEVEL_VALUE, -1,
                       COLUMN_IMAGE_LEVEL_TEXT, _("Auto"),
                       -1);
#if HAVE_GTK2_6
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COLUMN_IMAGE_LEVEL_VALUE, -2, /* -2 is magic separator value */
                       COLUMN_IMAGE_LEVEL_TEXT, "Separator",
                       -1);
#endif
    for (i = 0; i < levels; i++)
    {
        char *text;

        text = bugle_asprintf("%d", i);
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COLUMN_IMAGE_LEVEL_VALUE, i,
                           COLUMN_IMAGE_LEVEL_TEXT, text,
                           -1);
        bugle_free(text);
        if (i == old) new_iter = iter;
    }

    if (old < 0) /* No previous value or auto */
        gtk_combo_box_set_active(GTK_COMBO_BOX(viewer->level), 0);
    else if (old >= levels)  /* Not enough levels any more, take the last */
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(viewer->level), &iter);
    else
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(viewer->level), &new_iter);
}

void gldb_gui_image_viewer_update_face_zoffset(GldbGuiImageViewer *viewer)
{
    if (!viewer->current)
        return;

    switch (viewer->current->type)
    {
    case GLDB_GUI_IMAGE_TYPE_CUBE_MAP:
        if (viewer->zoffset)
            gtk_widget_set_sensitive(viewer->zoffset, FALSE);
        if (viewer->face)
        {
            gtk_widget_set_sensitive(viewer->face, TRUE);
            /* Updates the current_plane if necessary */
            g_signal_emit_by_name(G_OBJECT(viewer->face), "changed", viewer);
        }
        else
            viewer->current_plane = 0;
        break;
    case GLDB_GUI_IMAGE_TYPE_3D:
        if (viewer->face)
            gtk_widget_set_sensitive(viewer->face, FALSE);
        if (viewer->zoffset)
        {
            gtk_widget_set_sensitive(viewer->zoffset, TRUE);
            gtk_spin_button_set_range(GTK_SPIN_BUTTON(viewer->zoffset),
                                      0, viewer->current->levels[MAX(viewer->current_level, 0)].nplanes - 1);
            /* Updates the current_plane if necessary */
            g_signal_emit_by_name(G_OBJECT(viewer->zoffset), "value-changed", viewer);
        }
        else
            viewer->current_plane = 0;
        break;
    default:
        if (viewer->face)
            gtk_widget_set_sensitive(viewer->face, FALSE);
        if (viewer->zoffset)
            gtk_widget_set_sensitive(viewer->zoffset, FALSE);
        viewer->current_plane = 0;
        gldb_gui_image_viewer_update_zoom(viewer);
        break;
    }
}

void gldb_gui_image_viewer_update_min_filter(GldbGuiImageViewer *viewer,
                                             bugle_bool sensitive)
{
    gboolean valid;
    GtkTreeModel *model;
    GtkTreeIter iter;

    g_assert(viewer->min_filter != NULL);

    if (!sensitive)
    {
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(viewer->min_filter),
                                       viewer->min_filter_renderer,
                                       "text", COLUMN_IMAGE_FILTER_TEXT,
                                       "sensitive", COLUMN_IMAGE_FILTER_NON_MIP,
                                       NULL);
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(viewer->min_filter));
        valid = FALSE;
        if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(viewer->min_filter), &iter))
            gtk_tree_model_get(model, &iter,
                               COLUMN_IMAGE_FILTER_NON_MIP, &valid,
                               -1);
        if (!valid)
            gtk_combo_box_set_active(GTK_COMBO_BOX(viewer->min_filter), 0);
    }
    else
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(viewer->min_filter),
                                       viewer->min_filter_renderer,
                                       "text", COLUMN_IMAGE_FILTER_TEXT,
                                       NULL);
}

void gldb_gui_image_viewer_update_zoom(GldbGuiImageViewer *viewer)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    int width, height, level;
    const GldbGuiImagePlane *plane;
    gboolean more, valid;

    if (!viewer->current)
        return;

    level = MAX(viewer->current_level, 0);
    plane = &viewer->current->levels[level].planes[MAX(viewer->current_plane, 0)];
    width = MAX(plane->width, 1);
    height = MAX(plane->height, 1);
    if (viewer->current->type == GLDB_GUI_IMAGE_TYPE_CUBE_MAP
        && viewer->current_plane == -1)
        width *= 2; /* allow for two views of the cube */

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(viewer->zoom));
    more = gtk_tree_model_get_iter_first(model, &iter);
    while (more)
    {
        gdouble zoom;

        gtk_tree_model_get(model, &iter, COLUMN_IMAGE_ZOOM_VALUE, &zoom, -1);
        if (zoom > 0.0)
        {
            valid = zoom * width < viewer->max_viewport_dims[0]
                && zoom * height < viewer->max_viewport_dims[1]
                && zoom * width >= 1.0
                && zoom * height >= 1.0;
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COLUMN_IMAGE_ZOOM_SENSITIVE, valid, -1);
        }
        more = gtk_tree_model_iter_next(model, &iter);
    }
    valid = FALSE;
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(viewer->zoom), &iter))
        gtk_tree_model_get(model, &iter, COLUMN_IMAGE_ZOOM_SENSITIVE, &valid, -1);
    /* FIXME: pick nearest zoom level rather than "Fit" */
    if (!valid)
        gtk_combo_box_set_active(GTK_COMBO_BOX(viewer->zoom), 0);
    else
        /* Updates the sensitivity of the zoom buttons if necessary */
        g_signal_emit_by_name(G_OBJECT(viewer->zoom), "changed", viewer);
}

void gldb_gui_image_plane_clear(GldbGuiImagePlane *plane)
{
    if (plane)
    {
        if (plane->owns_pixels && plane->pixels) bugle_free(plane->pixels);
        plane->pixels = NULL;
    }
}

void gldb_gui_image_level_clear(GldbGuiImageLevel *level)
{
    if (level)
    {
        int i;
        for (i = 0; i < level->nplanes; i++)
            gldb_gui_image_plane_clear(&level->planes[i]);
        if (level->planes)
            bugle_free(level->planes);
        level->planes = NULL;
        level->nplanes = 0;
    }
}

void gldb_gui_image_clear(GldbGuiImage *image)
{
    if (image)
    {
        int i;
        for (i = 0; i < image->nlevels; i++)
            gldb_gui_image_level_clear(&image->levels[i]);
        if (image->levels)
            bugle_free(image->levels);
        image->levels = NULL;
        image->nlevels = 0;
    }
}

void gldb_gui_image_allocate(GldbGuiImage *image, GldbGuiImageType type,
                             int nlevels, int nplanes)
{
    int i;

    image->type = type;
    switch (type)
    {
    case GLDB_GUI_IMAGE_TYPE_2D:
        image->texture_target = GL_TEXTURE_2D;
        break;
    case GLDB_GUI_IMAGE_TYPE_3D:
        image->texture_target = GL_TEXTURE_3D_EXT;
        break;
    case GLDB_GUI_IMAGE_TYPE_CUBE_MAP:
        image->texture_target = GL_TEXTURE_CUBE_MAP_EXT;
        break;
    default:
        g_error("Image type is not supported - update glext.h and recompile");
    }

    image->nlevels = nlevels;
    if (nlevels)
        image->levels = BUGLE_NMALLOC(nlevels, GldbGuiImageLevel);
    else
        image->levels = NULL;
    for (i = 0; i < nlevels; i++)
    {
        image->levels[i].nplanes = nplanes;
        if (nplanes)
            image->levels[i].planes = BUGLE_CALLOC(nplanes, GldbGuiImagePlane);
        else
            image->levels[i].planes = NULL;
    }
}

/* Rounds up to a power of 2 */
static int round_up_two(int x)
{
    int y = 1;
    while (y < x)
        y *= 2;
    return y;
}

void gldb_gui_image_upload(GldbGuiImage *image, bugle_bool remap)
{
    GLenum face, format;
    int l, p;
    bugle_bool have_npot;
    GLint texture_width, texture_height, texture_depth;
    GldbGuiImagePlane *plane;

    if (remap)
    {
        GLfloat low = HUGE_VAL, high = -HUGE_VAL;
        GLfloat scale, bias;

        for (l = 0; l < image->nlevels; l++)
            for (p = 0; p < image->levels[l].nplanes; p++)
            {
                int channels, x, y, c;
                plane = &image->levels[l].planes[p];
                channels = gldb_channel_count(plane->channels);
                for (y = 0; y < plane->height; y++)
                    for (x = 0; x < plane->width; x++)
                        for (c = 0; c < channels; c++)
                        {
                            GLfloat value;
                            value = gldb_gui_image_plane_get_pixel(plane, x, y, c);
                            low = MIN(low, value);
                            high = MAX(high, value);
                        }
            }
        if (high == HUGE_VAL || high - low < 1e-8)
            remap = BUGLE_FALSE;

        scale = 1.0f / (high - low);
        bias = -low * scale;
        glPixelTransferf(GL_RED_SCALE, scale);
        glPixelTransferf(GL_GREEN_SCALE, scale);
        glPixelTransferf(GL_BLUE_SCALE, scale);
        glPixelTransferf(GL_ALPHA_SCALE, scale);
        glPixelTransferf(GL_RED_BIAS, bias);
        glPixelTransferf(GL_GREEN_BIAS, bias);
        glPixelTransferf(GL_BLUE_BIAS, bias);
        glPixelTransferf(GL_ALPHA_BIAS, bias);
    }

    have_npot = gdk_gl_query_gl_extension("GL_ARB_texture_non_power_of_two");
    switch (image->type)
    {
    case GLDB_GUI_IMAGE_TYPE_2D:
        for (l = 0; l < image->nlevels; l++)
        {
            plane = &image->levels[l].planes[0];
            texture_width = have_npot ? plane->width : round_up_two(plane->width);
            texture_height = have_npot ? plane->height : round_up_two(plane->height);
            format = gldb_channel_get_display_token(plane->channels);
            glTexImage2D(image->texture_target, l, format,
                         texture_width, texture_height,
                         0, format, plane->type, NULL);
            glTexSubImage2D(image->texture_target, l,
                            0, 0, plane->width, plane->height,
                            format, plane->type, plane->pixels);
            if (l == 0)
            {
                image->s = (GLfloat) plane->width / texture_width;
                image->t = (GLfloat) plane->height / texture_height;
            }
        }
        break;
#ifdef GL_ARB_texture_cube_map
    case GLDB_GUI_IMAGE_TYPE_CUBE_MAP:
        if (gdk_gl_query_gl_extension("GL_ARB_texture_cube_map"))
        {
            for (l = 0; l < image->nlevels; l++)
                for (p = 0; p < 6; p++)
                {
                    face = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + p;
                    GldbGuiImagePlane *plane = &image->levels[l].planes[p];
                    texture_width = have_npot ? plane->width : round_up_two(plane->width);
                    texture_height = have_npot ? plane->height : round_up_two(plane->height);
                    format = gldb_channel_get_display_token(plane->channels);
                    glTexImage2D(face, l, format,
                                 texture_width, texture_height,
                                 0, format, GL_FLOAT, NULL);
                    glTexSubImage2D(face, l,
                                    0, 0, plane->width, plane->height,
                                    format, GL_FLOAT, plane->pixels);
                    if (l == 0 && p == 0)
                    {
                        image->s = (GLfloat) plane->width / texture_width;
                        image->t = (GLfloat) plane->height / texture_height;
                    }
                }
        }
        break;
#endif
#ifdef GL_EXT_texture3D
    case GLDB_GUI_IMAGE_TYPE_3D:
        if (gdk_gl_query_gl_extension("GL_EXT_texture3D"))
        {
            for (l = 0; l < image->nlevels; l++)
            {
                int depth = image->levels[l].nplanes;
                GldbGuiImagePlane *plane = &image->levels[l].planes[0];
                texture_width = have_npot ? plane->width : round_up_two(plane->width);
                texture_height = have_npot ? plane->height : round_up_two(plane->height);
                texture_depth = have_npot ? depth : round_up_two(depth);
                format = gldb_channel_get_display_token(plane->channels);
                glTexImage3D(image->texture_target, l, format,
                             texture_width, texture_height, texture_depth,
                             0, format, GL_FLOAT, NULL);
                for (p = 0; p < depth; p++)
                {
                    GldbGuiImagePlane *plane2 = &image->levels[l].planes[p];
                    glTexSubImage3D(image->texture_target, l,
                                    0, 0, p, plane2->width, plane2->height, 1,
                                    format, GL_FLOAT, plane2->pixels);
                }
                if (l == 0)
                {
                    image->s = (GLfloat) plane->width / texture_width;
                    image->t = (GLfloat) plane->height / texture_height;
                    image->r = (GLfloat) depth / texture_depth;
                }
            }
        }
        else
            g_error("3D textures not supported");
        break;
#endif
    default:
        g_error("Image type not supported - please update glext.h");
    }

    if (remap)
    {
        glPixelTransferf(GL_RED_SCALE, 1.0f);
        glPixelTransferf(GL_GREEN_SCALE, 1.0f);
        glPixelTransferf(GL_BLUE_SCALE, 1.0f);
        glPixelTransferf(GL_ALPHA_SCALE, 1.0f);
        glPixelTransferf(GL_RED_BIAS, 0.0f);
        glPixelTransferf(GL_GREEN_BIAS, 0.0f);
        glPixelTransferf(GL_BLUE_BIAS, 0.0f);
        glPixelTransferf(GL_ALPHA_BIAS, 0.0f);
    }
}

static GtkTreeModel *build_filter_model(bugle_bool mag)
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

    store = gtk_list_store_new(3, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_BOOLEAN);
    count = mag ? 2 : 6;
    for (i = 0; i < count; i++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_IMAGE_FILTER_VALUE, filters[i].value,
                           COLUMN_IMAGE_FILTER_TEXT, filters[i].text,
                           COLUMN_IMAGE_FILTER_NON_MIP, filters[i].non_mip,
                           -1);
    }
    return GTK_TREE_MODEL(store);
}

static GtkTreeModel *build_face_model()
{
    GtkListStore *store;
    GtkTreeIter iter;
    static const char *names[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
    int i;

    store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_IMAGE_FACE_VALUE, -1,  /* magic "all" value */
                       COLUMN_IMAGE_FACE_TEXT, "All",
                       -1);
#if HAVE_GTK2_6
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_IMAGE_FACE_VALUE, -2,  /* magic separator value */
                       COLUMN_IMAGE_FACE_TEXT, "Separator",
                       -1);
#endif

    for (i = 0; i < 6; i++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_IMAGE_FACE_VALUE, i,
                           COLUMN_IMAGE_FACE_TEXT, names[i],
                           -1);
    }
    return GTK_TREE_MODEL(store);
}

void gldb_gui_image_initialise(void)
{
    mag_filter_model = build_filter_model(BUGLE_TRUE);
    min_filter_model = build_filter_model(BUGLE_FALSE);
    face_model = build_face_model();
}

void gldb_gui_image_shutdown(void)
{
    g_object_unref(mag_filter_model);
    g_object_unref(min_filter_model);
}

#endif /* HAVE_GTKGLEXT */
