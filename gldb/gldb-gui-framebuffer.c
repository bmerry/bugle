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

/* Functions in gldb-gui that deal with the framebuffer viewer. Generic image
 * handling code is in gldb-gui-image.c
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
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtkgl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <bugle/porting.h>
#include "common/protocol.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-channels.h"
#include "gldb/gldb-gui.h"
#include "gldb/gldb-gui-image.h"
#include "gldb/gldb-gui-framebuffer.h"
#include "xalloc.h"
#include "xvasprintf.h"

struct _GldbFramebufferPane
{
    GldbPane parent;

    /* private */
    gboolean dirty;
    GtkWidget *top_widget;
    GtkWidget *id, *buffer;

    GldbGuiImage active;
    GldbGuiImageViewer *viewer;
};

struct _GldbFramebufferPaneClass
{
    GldbPaneClass parent;
};

enum
{
    COLUMN_FRAMEBUFFER_ID_ID,
    COLUMN_FRAMEBUFFER_ID_TARGET,
    COLUMN_FRAMEBUFFER_ID_BOLD,
    COLUMN_FRAMEBUFFER_ID_TEXT
};

enum
{
    COLUMN_FRAMEBUFFER_BUFFER_ID,
    COLUMN_FRAMEBUFFER_BUFFER_CHANNELS,
    COLUMN_FRAMEBUFFER_BUFFER_TEXT
};

typedef struct
{
    GldbFramebufferPane *pane;
    uint32_t channels;
    guint32 flags;  /* not used at present */
} framebuffer_callback_data;

static gboolean gldb_framebuffer_pane_response_callback(gldb_response *response,
                                                        gpointer user_data)
{
    GldbFramebufferPane *pane;
    gldb_response_data_framebuffer *r;
    framebuffer_callback_data *data;
    GLenum format;
    uint32_t channels;
    GLint width, height;

    r = (gldb_response_data_framebuffer *) response;
    data = (framebuffer_callback_data *) user_data;
    pane = data->pane;

    gldb_gui_image_clear(&pane->active);
    pane->viewer->current = NULL;
    if (response->code != RESP_DATA || !r->length)
    {
        /* FIXME: tag the framebuffer as invalid and display error */
    }
    else
    {
        GdkGLContext *glcontext;
        GdkGLDrawable *gldrawable;

        channels = data->channels;
        format = gldb_channel_get_display_token(channels);
        width = r->width;
        height = r->height;

        /* Save our copy of the data */
        gldb_gui_image_allocate(&pane->active,
                                GLDB_GUI_IMAGE_TYPE_2D, 1, 1);
        pane->active.levels[0].planes[0].width = width;
        pane->active.levels[0].planes[0].height = height;
        pane->active.levels[0].planes[0].channels = channels;
        pane->active.levels[0].planes[0].owns_pixels = true;
        pane->active.levels[0].planes[0].pixels = (GLfloat *) r->data;
        r->data = NULL; /* stops gldb_free_response from freeing data */

        glcontext = gtk_widget_get_gl_context(pane->viewer->draw);
        gldrawable = gtk_widget_get_gl_drawable(pane->viewer->draw);
        if (gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        {
            if (gdk_gl_query_gl_extension("GL_SGIS_generate_mipmap"))
            {
                glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
                pane->viewer->texture_min_filter = GL_LINEAR_MIPMAP_LINEAR;
            }
            else
                pane->viewer->texture_min_filter = GL_LINEAR;
            gldb_gui_image_upload(&pane->active,
                                  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pane->viewer->remap)));
            gdk_gl_drawable_gl_end(gldrawable);
        }

        /* Prepare for rendering */
        pane->viewer->current = &pane->active;
        pane->viewer->current_level = 0;
        pane->viewer->current_plane = 0;
    }

    gldb_gui_image_viewer_update_zoom(pane->viewer);
    gtk_widget_queue_draw(pane->viewer->draw);
    gldb_free_response(response);
    return TRUE;
}

static void gldb_framebuffer_pane_update_ids(GldbFramebufferPane *pane)
{
    GValue old[2];
    GtkTreeIter iter;
    gldb_state *root, *framebuffer, *binding;
    GtkTreeModel *model;
    char *name;
    GLint active = -1;

    root = gldb_state_update();
    g_return_if_fail(root != NULL);

    gldb_gui_combo_box_get_old(GTK_COMBO_BOX(pane->id), old,
                               COLUMN_FRAMEBUFFER_ID_ID, COLUMN_FRAMEBUFFER_ID_TARGET, -1);
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->id));
    gtk_list_store_clear(GTK_LIST_STORE(model));

    binding = gldb_state_find_child_enum(root, GL_FRAMEBUFFER_BINDING_EXT);
    if (binding)
        active = gldb_state_GLint(binding);
    if ((framebuffer = gldb_state_find_child_enum(root, GL_FRAMEBUFFER_EXT)) != NULL)
    {
        linked_list_node *pfbo;
        gldb_state *fbo;

        for (pfbo = bugle_list_head(&framebuffer->children); pfbo != NULL; pfbo = bugle_list_next(pfbo))
        {
            fbo = (gldb_state *) bugle_list_data(pfbo);

            if (fbo->numeric_name == 0)
                name = xstrdup(_("Default"));
            else
                name = xasprintf("%u", (unsigned int) fbo->numeric_name);
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COLUMN_FRAMEBUFFER_ID_ID, fbo->numeric_name,
                               COLUMN_FRAMEBUFFER_ID_TARGET, GL_FRAMEBUFFER_EXT,
                               COLUMN_FRAMEBUFFER_ID_BOLD, fbo->numeric_name == active ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
                               COLUMN_FRAMEBUFFER_ID_TEXT, name,
                               -1);
            free(name);
        }
    }
    else
    {
        gtk_list_store_append(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COLUMN_FRAMEBUFFER_ID_ID, 0,
                           COLUMN_FRAMEBUFFER_ID_TARGET, 0,
                           COLUMN_FRAMEBUFFER_ID_BOLD, PANGO_WEIGHT_BOLD,
                           COLUMN_FRAMEBUFFER_ID_TEXT, _("Default"),
                           -1);
    }

    gldb_gui_combo_box_restore_old(GTK_COMBO_BOX(pane->id), old,
                                   COLUMN_FRAMEBUFFER_ID_ID, COLUMN_FRAMEBUFFER_ID_TARGET, -1);
}

static void gldb_framebuffer_pane_id_changed(GtkWidget *widget, gpointer user_data)
{
    GldbFramebufferPane *pane;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean stereo = FALSE, doublebuffer = FALSE;
    guint channels = 0, color_channels;
    guint id;
    GValue old_buffer;
    gldb_state *root, *framebuffer, *fbo, *parameter;
    int i, attachments;
    char *name;

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
    {
        pane = (GldbFramebufferPane *) user_data;
        root = gldb_state_update();

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->id));
        if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(pane->id), &iter))
            return;
        gtk_tree_model_get(model, &iter,
                           COLUMN_FRAMEBUFFER_ID_ID, &id,
                           -1);

        gldb_gui_combo_box_get_old(GTK_COMBO_BOX(pane->buffer), &old_buffer,
                                   COLUMN_FRAMEBUFFER_BUFFER_ID, -1);
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->buffer));
        gtk_list_store_clear(GTK_LIST_STORE(model));

        framebuffer = gldb_state_find_child_enum(root, GL_FRAMEBUFFER_EXT);
        if (id != 0)
        {
            g_assert(framebuffer != NULL);
            fbo = gldb_state_find_child_numeric(framebuffer, id);
            g_assert(fbo != NULL);

            for (i = 0; gldb_channel_table[i].channel; i++)
                if (gldb_channel_table[i].framebuffer_size_token
                    && (parameter = gldb_state_find_child_enum(fbo, gldb_channel_table[i].framebuffer_size_token)) != NULL
                    && gldb_state_GLint(parameter) != 0)
                {
                    channels |= gldb_channel_table[i].channel;
                }
            color_channels = gldb_channel_get_query_channels(channels & ~GLDB_CHANNEL_DEPTH_STENCIL);

            parameter = gldb_state_find_child_enum(fbo, GL_MAX_COLOR_ATTACHMENTS_EXT);
            g_assert(parameter != NULL);
            attachments = gldb_state_GLint(parameter);
            for (i = 0; i < attachments; i++)
            {
                if (gldb_state_find_child_enum(fbo, GL_COLOR_ATTACHMENT0_EXT + i))
                {
                    name = xasprintf(_("Color %d"), i);
                    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                       COLUMN_FRAMEBUFFER_BUFFER_ID, (guint) (GL_COLOR_ATTACHMENT0_EXT + i),
                                       COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, color_channels,
                                       COLUMN_FRAMEBUFFER_BUFFER_TEXT, name,
                                       -1);
                    free(name);
                }
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
        }
        else
        {
            if (!framebuffer
                || (fbo = gldb_state_find_child_numeric(framebuffer, 0)) == NULL)
                fbo = root;
            for (i = 0; gldb_channel_table[i].channel; i++)
                if (gldb_channel_table[i].framebuffer_size_token
                    && (parameter = gldb_state_find_child_enum(fbo, gldb_channel_table[i].framebuffer_size_token)) != NULL
                    && gldb_state_GLint(parameter) != 0)
                {
                    channels |= gldb_channel_table[i].channel;
                }
            color_channels = gldb_channel_get_query_channels(channels & ~GLDB_CHANNEL_DEPTH_STENCIL);

            if ((parameter = gldb_state_find_child_enum(fbo, GL_DOUBLEBUFFER)) != NULL
                && gldb_state_GLboolean(parameter))
                doublebuffer = TRUE;
            if ((parameter = gldb_state_find_child_enum(fbo, GL_STEREO)) != NULL
                && gldb_state_GLboolean(parameter))
                stereo = TRUE;

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
            if ((parameter = gldb_state_find_child_enum(fbo, GL_AUX_BUFFERS)) != NULL)
            {
                attachments = gldb_state_GLint(parameter);
                for (i = 0; i < attachments; i++)
                {
                    name = xasprintf(_("GL_AUX%d"), i);
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
             * need to use a unique identifier for
             * gldb_gui_combo_box_restore_old to work as intended.
             * We filter out these illegal values later.
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
        gldb_gui_combo_box_restore_old(GTK_COMBO_BOX(pane->buffer), &old_buffer,
                                       COLUMN_FRAMEBUFFER_BUFFER_ID, -1);
    }
}

static void gldb_framebuffer_pane_buffer_changed(GtkWidget *widget, gpointer user_data)
{
    GldbFramebufferPane *pane;
    framebuffer_callback_data *data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    guint id, target, buffer, channel;
    guint32 seq;
    GLenum type, format;

    pane = (GldbFramebufferPane *) user_data;

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
    {
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->id));
        if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(pane->id), &iter))
            return;
        gtk_tree_model_get(model, &iter,
                           COLUMN_FRAMEBUFFER_ID_ID, &id,
                           COLUMN_FRAMEBUFFER_ID_TARGET, &target,
                           -1);
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->buffer));
        if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(pane->buffer), &iter))
            return;
        gtk_tree_model_get(model, &iter,
                           COLUMN_FRAMEBUFFER_BUFFER_ID, &buffer,
                           COLUMN_FRAMEBUFFER_BUFFER_CHANNELS, &channel,
                           -1);

        if (!id && (channel & GLDB_CHANNEL_DEPTH_STENCIL))
        {
            /* We used an illegal but unique value for id to assist
             * gldb_gui_combo_box_restore_old. Put in GL_FRONT, which is
             * always legal. The debugger filter-set may eventually ignore
             * the value.
             */
            buffer = GL_FRONT;
        }

        data = XMALLOC(framebuffer_callback_data);
        data->channels = gldb_channel_get_query_channels(channel);
        data->flags = 0;
        data->pane = pane;
        seq = gldb_gui_set_response_handler(gldb_framebuffer_pane_response_callback, data);

#if BUGLE_GLTYPE_GL
        format = gldb_channel_get_framebuffer_token(data->channels);
        type = GL_FLOAT;
#elif BUGLE_GLTYPE_GLES2
        /* GL ES only supports this combination plus an implementation-defined one */
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
#else
# error "Unknown target"
#endif
        gldb_send_data_framebuffer(seq, id, target, buffer, format, type);
    }
}

static GtkWidget *gldb_framebuffer_pane_id_new(GldbFramebufferPane *pane)
{
    GtkListStore *store;
    GtkWidget *id;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(4,
                               G_TYPE_UINT,  /* ID */
                               G_TYPE_UINT,  /* target */
                               G_TYPE_INT,   /* bold */
                               G_TYPE_STRING);

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
                                         COLUMN_FRAMEBUFFER_ID_ID,
                                         GTK_SORT_ASCENDING);
    id = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_widget_set_size_request(id, 80, -1);

    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(id), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(id), cell,
                                   "text", COLUMN_FRAMEBUFFER_ID_TEXT,
                                   "weight", COLUMN_FRAMEBUFFER_ID_BOLD,
                                   NULL);
    g_signal_connect(G_OBJECT(id), "changed",
                     G_CALLBACK(gldb_framebuffer_pane_id_changed), pane);

    g_object_unref(G_OBJECT(store));
    return pane->id = id;
}

static GtkWidget *gldb_framebuffer_pane_buffer_new(GldbFramebufferPane *pane)
{
    GtkListStore *store;
    GtkWidget *buffer;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(3,
                               G_TYPE_UINT,    /* id */
                               G_TYPE_UINT,    /* channels */
                               G_TYPE_STRING); /* text */

    buffer = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(buffer), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(buffer), cell,
                                   "text", COLUMN_FRAMEBUFFER_BUFFER_TEXT,
                                   NULL);
    g_signal_connect(G_OBJECT(buffer), "changed",
                     G_CALLBACK(gldb_framebuffer_pane_buffer_changed), pane);
    g_object_unref(G_OBJECT(store));
    return pane->buffer = buffer;
}

static GtkWidget *gldb_framebuffer_pane_combo_table_new(GldbFramebufferPane *pane)
{
    GtkWidget *combos;
    GtkWidget *label, *id, *buffer, *zoom, *remap;

    combos = gtk_table_new(5, 2, FALSE);

    label = gtk_label_new(_("Framebuffer"));
    id = gldb_framebuffer_pane_id_new(pane);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 0, 1, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), id, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Buffer"));
    buffer = gldb_framebuffer_pane_buffer_new(pane);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 1, 2, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), buffer, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Zoom"));
    zoom = pane->viewer->zoom;
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 3, 4, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), zoom, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    remap = gldb_gui_image_viewer_remap_new(pane->viewer);
    gtk_table_attach(GTK_TABLE(combos), remap, 0, 2, 4, 5, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    return combos;
}

static GtkWidget *gldb_framebuffer_pane_toolbar_new(GldbFramebufferPane *pane)
{
    GtkWidget *toolbar;
    GtkToolItem *item;

    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

    item = pane->viewer->copy;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    item = pane->viewer->zoom_in;
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = pane->viewer->zoom_out;
    gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = pane->viewer->zoom_100;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    item = pane->viewer->zoom_fit;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    return toolbar;
}

GldbPane *gldb_framebuffer_pane_new(GtkStatusbar *statusbar,
                                    guint statusbar_context_id)
{
    GldbFramebufferPane *pane;
    GtkWidget *hbox, *vbox, *combos, *scrolled, *toolbar;

    pane = GLDB_FRAMEBUFFER_PANE(g_object_new(GLDB_FRAMEBUFFER_PANE_TYPE, NULL));
    pane->viewer = gldb_gui_image_viewer_new(statusbar, statusbar_context_id);
    combos = gldb_framebuffer_pane_combo_table_new(pane);
    toolbar = gldb_framebuffer_pane_toolbar_new(pane);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
                                          pane->viewer->alignment);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), combos, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    gldb_pane_set_widget(GLDB_PANE(pane), vbox);
    return GLDB_PANE(pane);
}

static void gldb_framebuffer_pane_real_update(GldbPane *self)
{
    GldbFramebufferPane *framebuffer;

    framebuffer = GLDB_FRAMEBUFFER_PANE(self);
    /* Simply invoke a change event on the selector. This launches a
     * cascade of updates.
     */
    gldb_framebuffer_pane_update_ids(framebuffer);
}

/* GObject stuff */

static void gldb_framebuffer_pane_class_init(GldbFramebufferPaneClass *klass)
{
    GldbPaneClass *pane_class;

    pane_class = GLDB_PANE_CLASS(klass);
    pane_class->do_real_update = gldb_framebuffer_pane_real_update;
}

static void gldb_framebuffer_pane_init(GldbFramebufferPane *self, gpointer g_class)
{
    self->id = NULL;
    self->buffer = NULL;
    self->viewer = NULL;

    memset(&self->active, 0, sizeof(self->active));
}

GType gldb_framebuffer_pane_get_type(void)
{
    static GType type = 0;
    if (type == 0)
    {
        static const GTypeInfo info =
        {
            sizeof(GldbFramebufferPaneClass),
            NULL,                       /* base_init */
            NULL,                       /* base_finalize */
            (GClassInitFunc) gldb_framebuffer_pane_class_init,
            NULL,                       /* class_finalize */
            NULL,                       /* class_data */
            sizeof(GldbFramebufferPane),
            0,                          /* n_preallocs */
            (GInstanceInitFunc) gldb_framebuffer_pane_init,
            NULL                        /* value table */
        };
        type = g_type_register_static(GLDB_PANE_TYPE,
                                      "GldbFramebufferPaneType",
                                      &info, 0);
    }
    return type;
}

#endif /* HAVE_GTKGLEXT */
