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
    /* FIXME: check for existence of clamp_to_edge first */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    /* FIXME: convert to mipmapping when supported by framebuffers */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glEnable(GL_TEXTURE_2D);
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
    if (viewer->current && viewer->current->pixels)
    {
        s = (GLfloat) viewer->current->width / viewer->texture_width;
        t = (GLfloat) viewer->current->height / viewer->texture_height;

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(s, 0.0f);
        glVertex2f(1.0f, -1.0f);
        glTexCoord2f(s, t);
        glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, t);
        glVertex2f(-1.0f, 1.0f);
    }
    gdk_gl_drawable_swap_buffers(gldrawable);
    gdk_gl_drawable_gl_end(gldrawable);
    return TRUE;
}

static void image_draw_motion_clear_status(GldbGuiImageViewer *viewer)
{
    if (viewer->pixel_status_id != -1)
    {
        gtk_statusbar_remove(viewer->statusbar,
                             viewer->statusbar_context_id,
                             viewer->pixel_status_id);
        viewer->pixel_status_id = -1;
    }
}

static void image_draw_motion_update(GldbGuiImageViewer *viewer,
                                     GtkWidget *draw,
                                     gdouble x, gdouble y)
{
    int width, height;
    int u, v;
    GLfloat *pixel;
    char *msg, *tmp_msg;
    int nchannels, p;
    uint32_t channels, channel;

    image_draw_motion_clear_status(viewer);
    /* FIXME: handle cube and 3D textures */
    if (!viewer->current || !viewer->current->pixels)
        return;

    width = viewer->current->width;
    height = viewer->current->height;
    x = (x + 0.5) / draw->allocation.width * width;
    y = (1.0 - (y + 0.5) / draw->allocation.height) * height;
    u = CLAMP((int) x, 0, width - 1);
    v = CLAMP((int) y, 0, height - 1);
    nchannels = gldb_channel_count(viewer->current->channels);
    pixel = viewer->current->pixels + nchannels * (v * width + u);
    bugle_asprintf(&msg, "u: %d v: %d ", u, v);

    channels = viewer->current->channels;
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
    if (viewer->pixel_status_id != -1)
    {
        gtk_statusbar_remove(viewer->statusbar,
                             viewer->statusbar_context_id,
                             viewer->pixel_status_id);
        viewer->pixel_status_id = -1;
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

    if (!viewer->current || !viewer->current->pixels)
        return;

    draw = viewer->draw;
    aspect = gtk_widget_get_parent(draw);
    alignment = viewer->alignment;
    width = viewer->current->width;
    height = viewer->current->height;

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

    /* FIXME: support cube map and 3D */

    viewer = (GldbGuiImageViewer *) user_data;
    if (!viewer->current || !viewer->current->pixels)
        return;

    width = viewer->current->width;
    height = viewer->current->height;
    nin = gldb_channel_count(viewer->current->channels);
    nout = CLAMP(nin, 3, 4);
    pixels = bugle_malloc(width * height * nout * sizeof(guint8));
    p = pixels;
    /* FIXME: disable button when appropriate */
    for (y = height - 1; y >= 0; y--)
        for (x = 0; x < width; x++)
            for (c = 0; c < nout; c++)
                *p++ = FLOAT_TO_UBYTE(viewer->current->pixels[((y * width) + x) * nin + (c % nin)]);

    pixbuf = gdk_pixbuf_new_from_data(pixels, GDK_COLORSPACE_RGB, TRUE, 8,
                                      width, height, width * 4,
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

GldbGuiImageViewer *gldb_gui_image_viewer_new(GtkStatusbar *statusbar,
                                              guint statusbar_context_id)
{
    GldbGuiImageViewer *viewer;
    GtkWidget *zoom, *zoom_in, *zoom_out, *zoom_100;
    GtkWidget *copy;

    viewer = (GldbGuiImageViewer *) bugle_malloc(sizeof(GldbGuiImageViewer));
    viewer->current = NULL;
    viewer->statusbar = statusbar;
    viewer->statusbar_context_id = statusbar_context_id;
    gldb_gui_image_viewer_new_draw(viewer);
    gldb_gui_image_viewer_new_zoom_combo(viewer);
    gldb_gui_image_viewer_new_toolbuttons(viewer);

    return viewer;
}

#endif /* HAVE_GTKGLEXT */
