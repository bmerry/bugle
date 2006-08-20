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
#include "glee/GLee.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <gtk/gtkgl.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "common/safemem.h"
#include "common/bool.h"
#include "src/names.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-channels.h"
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
    COLUMN_IMAGE_FILTER_VALUE,
    COLUMN_IMAGE_FILTER_TEXT,
    COLUMN_IMAGE_FILTER_NON_MIP
};

static GtkTreeModel *mag_filter_model, *min_filter_model;

static void image_draw_realize(GtkWidget *widget, gpointer user_data)
{
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;
    GldbGuiImageViewer *viewer;

    viewer = (GldbGuiImageViewer *) user_data;
    glcontext = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) return;

    glViewport(0, 0, widget->allocation.width, widget->allocation.height);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    /* NVIDIA do not expose GL_SGIS_texture_edge_clamp (instead they
     * expose the unregistered GL_EXT_texture_edge_clamp), so we use
     * the OpenGL version as the test instead.
     */
#ifdef GL_VERSION_1_2
    if (strcmp(glGetString(GL_VERSION), "1.2") >= 0)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
    else
#endif
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#ifdef GL_EXT_texture3D
        if (gdk_gl_query_gl_extension("GL_EXT_texture3D"))
        {
            glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_WRAP_R_EXT, GL_CLAMP);
        }
#endif
    }
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewer->max_viewport_dims);

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

/* FIXME: handle cube-map and 3D textures */
static gboolean image_draw_expose(GtkWidget *widget,
                                  GdkEventExpose *event,
                                  gpointer user_data)
{
    GldbGuiImageViewer *viewer;
    GdkGLContext *glcontext;
    GdkGLDrawable *gldrawable;
    GLfloat s, t;

    glcontext = gtk_widget_get_gl_context(widget);
    gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext)) return FALSE;
    glClear(GL_COLOR_BUFFER_BIT);

    viewer = (GldbGuiImageViewer *) user_data;
    if (viewer->current)
    {
        glEnable(viewer->current->texture_target);
#ifdef GL_VERSION_1_2
        if (strcmp(glGetString(GL_VERSION), "1.2") >= 0)
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
#endif
        glTexParameteri(viewer->current->texture_target, GL_TEXTURE_MAG_FILTER, viewer->texture_mag_filter);
        glTexParameteri(viewer->current->texture_target, GL_TEXTURE_MIN_FILTER, viewer->texture_min_filter);

        s = viewer->current->s;
        t = viewer->current->t;

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(s, 0.0f);
        glVertex2f(1.0f, -1.0f);
        glTexCoord2f(s, t);
        glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, t);
        glVertex2f(-1.0f, 1.0f);
        glEnd();

        glDisable(viewer->current->texture_target);
    }
    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);
    return TRUE;
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
    /* FIXME: handle cube and 3D textures */
    if (!viewer->current)
        return;
    if (viewer->current_plane < 0)
        return;

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
    bugle_asprintf(&msg, "u: %d v: %d ", u, v);

    channels = plane->channels;
    for (p = 0; channels; channels &= ~channel, p++)
    {
        channel = channels & ~(channels - 1);
        const char *abbr = gldb_channel_get_abbr(channel);
        tmp_msg = msg;
        bugle_asprintf(&msg, " %s %s: %f", tmp_msg, abbr ? abbr : "?", pixel[p]);
        free(tmp_msg);
    }
    viewer->pixel_status_id = gtk_statusbar_push(viewer->statusbar,
                                                 viewer->statusbar_context_id,
                                                 msg);
    free(msg);
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

static void *gldb_gui_image_viewer_new_draw(GldbGuiImageViewer *viewer)
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

static gboolean zoom_row_separator(GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   gpointer data)
{
    gdouble value;

    gtk_tree_model_get(model, iter, COLUMN_IMAGE_ZOOM_VALUE, &value, -1);
    return value == -2.0;
}

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
            gtk_tree_model_get(model, &iter, COLUMN_IMAGE_ZOOM_SENSITIVE, &sensitive_out, -1);
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

    /* Note: separator must be non-sensitive, because the zoom-out button
     * examines its sensitivity to see if it can zoom out further.
     */
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_IMAGE_ZOOM_VALUE, -2.0, /* -2.0 is magic separator value - see above function */
                       COLUMN_IMAGE_ZOOM_TEXT, "Separator",
                       COLUMN_IMAGE_ZOOM_SENSITIVE, FALSE,
                       -1);

    for (i = 5; i >= 0; i--)
    {
        gchar *caption;

        bugle_asprintf(&caption, "1:%d", (1 << i));
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_IMAGE_ZOOM_VALUE, (gdouble) 1.0 / (1 << i),
                           COLUMN_IMAGE_ZOOM_TEXT, caption,
                           COLUMN_IMAGE_ZOOM_SENSITIVE, FALSE,
                           -1);
        free(caption);
    }
    for (i = 1; i <= 5; i++)
    {
        gchar *caption;

        bugle_asprintf(&caption, "%d:1", (1 << i));
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_IMAGE_ZOOM_VALUE, (gdouble) (1 << i),
                           COLUMN_IMAGE_ZOOM_TEXT, caption,
                           COLUMN_IMAGE_ZOOM_SENSITIVE, FALSE,
                           -1);
        free(caption);
    }

    viewer->zoom = zoom = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(zoom), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(zoom), cell,
                                   "text", COLUMN_IMAGE_ZOOM_TEXT,
                                   "sensitive", COLUMN_IMAGE_ZOOM_SENSITIVE,
                                   NULL);
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(zoom),
                                         zoom_row_separator,
                                         NULL, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(zoom), 0);
    g_signal_connect(G_OBJECT(zoom), "changed",
                     G_CALLBACK(image_zoom_changed), viewer);
    g_object_unref(G_OBJECT(store));
}

#define FLOAT_TO_UBYTE(x) ((guchar) CLAMP(floor((x) * 255.0), 0, 255))

static void free_pixbuf_data(guchar *pixels, gpointer user_data)
{
    free(pixels);
}

static void image_copy_clicked(GtkWidget *button, gpointer user_data)
{
    GLint width, height, nin, nout;
    guchar *pixels, *p;
    GdkPixbuf *pixbuf = NULL;
    GtkClipboard *clipboard;
    GldbGuiImageViewer *viewer;
    int y, x, c;
    int level;
    GldbGuiImagePlane *plane;

    /* FIXME: support cube map and 3D, or disable button when appropriate */

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
    pixels = bugle_malloc(width * height * nout * sizeof(guint8));
    p = pixels;
    for (y = height - 1; y >= 0; y--)
        for (x = 0; x < width; x++)
            for (c = 0; c < nout; c++)
                *p++ = FLOAT_TO_UBYTE(plane->pixels[((y * width) + x) * nin + (c % nin)]);

    pixbuf = gdk_pixbuf_new_from_data(pixels, GDK_COLORSPACE_RGB, nout == 4, 8,
                                      width, height, width * nout,
                                      free_pixbuf_data, NULL);
    if (!pixbuf) return;

    clipboard = gtk_clipboard_get_for_display(gtk_widget_get_display(button),
                                              GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_image(clipboard, pixbuf);
    g_object_unref(pixbuf);
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
    if (!viewer->current)
        return;
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

static gboolean level_row_separator(GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    gpointer user_data)
{
    gint value;

    gtk_tree_model_get(model, iter, COLUMN_IMAGE_LEVEL_VALUE, &value, -1);
    return value == -2;
}

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
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_IMAGE_LEVEL_VALUE, -2, /* -2 is magic separator value */
                       COLUMN_IMAGE_LEVEL_TEXT, "Separator",
                       -1);

    level = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(level), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(level), cell,
                                   "text", COLUMN_IMAGE_LEVEL_TEXT, NULL);
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(level),
                                         level_row_separator,
                                         NULL, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(level), 0);
    g_signal_connect(G_OBJECT(level), "changed",
                     G_CALLBACK(image_level_changed), viewer);
    g_object_unref(G_OBJECT(store));
    return viewer->level = level;
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

GtkWidget *gldb_gui_image_viewer_filter_new(GldbGuiImageViewer *viewer, bool mag)
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
    GtkWidget *zoom, *zoom_in, *zoom_out, *zoom_100;
    GtkWidget *copy;

    viewer = (GldbGuiImageViewer *) bugle_calloc(1, sizeof(GldbGuiImageViewer));
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
    GtkTreeIter iter;
    gint old;
    gint levels, i;

    if (!viewer->current)
        return;
    levels = viewer->current->nlevels;

    g_assert(viewer->level);
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(viewer->level));
    old = gtk_combo_box_get_active(GTK_COMBO_BOX(viewer->level));
    gtk_list_store_clear(GTK_LIST_STORE(model));
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COLUMN_IMAGE_LEVEL_VALUE, -1,
                       COLUMN_IMAGE_LEVEL_TEXT, _("Auto"),
                       -1);
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COLUMN_IMAGE_LEVEL_VALUE, -2, /* -2 is magic separator value */
                       COLUMN_IMAGE_LEVEL_TEXT, "Separator",
                       -1);
    for (i = 0; i < levels; i++)
    {
        char *text;

        bugle_asprintf(&text, "%d", i);
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COLUMN_IMAGE_LEVEL_VALUE, i,
                           COLUMN_IMAGE_LEVEL_TEXT, text,
                           -1);
        free(text);
    }
    if (old <= levels)
        gtk_combo_box_set_active(GTK_COMBO_BOX(viewer->level), old);
    else
        gtk_combo_box_set_active(GTK_COMBO_BOX(viewer->level), levels);
}

void gldb_gui_image_viewer_update_min_filter(GldbGuiImageViewer *viewer,
                                             bool sensitive)
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
        if (plane->owns_pixels && plane->pixels) free(plane->pixels);
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
            free(level->planes);
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
            free(image->levels);
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
#ifdef GL_EXT_texture3D
    case GLDB_GUI_IMAGE_TYPE_3D:
        image->texture_target = GL_TEXTURE_3D_EXT;
        break;
#endif
#ifdef GL_EXT_texture_cube
    case GLDB_GUI_IMAGE_TYPE_CUBE:
        image->texture_target = GL_TEXTURE_CUBE_MAP_EXT;
        break;
#endif
    default:
        g_error("Image type is not supported - update glext.h and recompile");
    }

    image->nlevels = nlevels;
    if (nlevels)
        image->levels = bugle_malloc(nlevels * sizeof(GldbGuiImageLevel));
    else
        image->levels = NULL;
    for (i = 0; i < nlevels; i++)
    {
        image->levels[i].nplanes = nplanes;
        if (nplanes)
            image->levels[i].planes = bugle_calloc(nplanes, sizeof(GldbGuiImagePlane));
        else
            image->levels[i].planes = NULL;
    }
}

static GtkTreeModel *build_filter_model(bool mag)
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

void gldb_gui_image_initialise(void)
{
    mag_filter_model = build_filter_model(true);
    min_filter_model = build_filter_model(false);
}

void gldb_gui_image_shutdown(void)
{
    g_object_unref(mag_filter_model);
    g_object_unref(min_filter_model);
}

#endif /* HAVE_GTKGLEXT */
