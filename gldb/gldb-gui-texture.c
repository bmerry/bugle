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

/* Functions in gldb-gui that deal with the texture viewer. Generic image
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
#include "glee/GLee.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtkgl.h>
#include <string.h>
#include "common/safemem.h"
#include "common/protocol.h"
#include "common/bool.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-channels.h"
#include "gldb/gldb-gui.h"
#include "gldb/gldb-gui-image.h"
#include "gldb/gldb-gui-texture.h"

#define TEXTURE_CALLBACK_FLAG_FIRST 1
#define TEXTURE_CALLBACK_FLAG_LAST 2

enum
{
    COLUMN_TEXTURE_ID_ID,
    COLUMN_TEXTURE_ID_TARGET,
    COLUMN_TEXTURE_ID_LEVELS,
    COLUMN_TEXTURE_ID_CHANNELS,
    COLUMN_TEXTURE_ID_TEXT
};

typedef struct
{
    GldbTexturePane *pane;

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

static gboolean gldb_texture_pane_response_callback(gldb_response *response,
                                                    gpointer user_data)
{
    gldb_response_data_texture *r;
    texture_callback_data *data;
    GLenum format;
    uint32_t channels;
    GldbGuiImageLevel *level;
    GldbTexturePane *pane;

    r = (gldb_response_data_texture *) response;
    data = (texture_callback_data *) user_data;
    pane = data->pane;
    if (data->flags & TEXTURE_CALLBACK_FLAG_FIRST)
    {
        gldb_gui_image_clear(&pane->progressive);
        gldb_gui_image_allocate(&pane->progressive, data->type,
                                data->nlevels, data->nplanes);
        pane->viewer->current = NULL;
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
        glcontext = gtk_widget_get_gl_context(pane->viewer->draw);
        gldrawable = gtk_widget_get_gl_drawable(pane->viewer->draw);

        if (gdk_gl_drawable_gl_begin(gldrawable, glcontext))
        {
            if (gdk_gl_query_gl_extension("GL_ARB_texture_non_power_of_two"))
            {
                texture_width = r->width;
                texture_height = r->height;
                texture_depth = r->depth;
            }
            else
            {
                texture_width = texture_height = texture_depth = 1;
                while (texture_width < r->width) texture_width *= 2;
                while (texture_height < r->height) texture_height *= 2;
                while (texture_depth < r->depth) texture_depth *= 2;
            }

            face = pane->progressive.texture_target;
            plane = 0;
            level = &pane->progressive.levels[data->level];
            switch (pane->progressive.type)
            {
            case GLDB_GUI_IMAGE_TYPE_CUBE_MAP:
                plane = data->face - GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
                face = data->face;
                /* Fall through */
                /* FIXME: test for cube map extension first */
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
                    pane->progressive.s = (GLfloat) r->width / texture_width;
                    pane->progressive.t = (GLfloat) r->height / texture_height;
                }
                break;
            case GLDB_GUI_IMAGE_TYPE_3D:
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
                        level->planes[plane].pixels = ((GLfloat *) r->data) + plane * r->width * r->height * gldb_channel_count(data->channels);
                    }
                    if (data->flags & TEXTURE_CALLBACK_FLAG_FIRST)
                    {
                        pane->progressive.s = (GLfloat) r->width / texture_width;
                        pane->progressive.t = (GLfloat) r->height / texture_height;
                        pane->progressive.r = (GLfloat) r->depth / texture_depth;
                    }
                    break;
                }
            default:
                g_error("Image type is not supported.");
            }
            gdk_gl_drawable_gl_end(gldrawable);
        }

        r->data = NULL; /* Prevents gldb_free_response from freeing the data */
    }

    if (data->flags & TEXTURE_CALLBACK_FLAG_LAST)
    {
        gldb_gui_image_clear(&pane->active);
        pane->active = pane->progressive;
        pane->viewer->current = &pane->active;
        memset(&pane->progressive, 0, sizeof(pane->progressive));

        gldb_gui_image_viewer_update_min_filter(pane->viewer,
                                                data->target != GL_TEXTURE_RECTANGLE_NV);
        gldb_gui_image_viewer_update_levels(pane->viewer);
        gldb_gui_image_viewer_update_face_zoffset(pane->viewer);
        gtk_widget_queue_draw(pane->viewer->draw);
    }

    gldb_free_response(response);
    free(data);
    return TRUE;
}

static void gldb_texture_pane_update_ids(GldbTexturePane *pane)
{
    GValue old[2];
    gldb_state *root, *s, *t, *l, *f, *param;
    GtkTreeModel *model;
    GtkTreeIter iter;
    bugle_list_node *nt, *nl;
    gchar *name;
    guint levels;
    uint32_t channels;

    const GLenum targets[] = {
        GL_TEXTURE_1D,
        GL_TEXTURE_2D,
        GL_TEXTURE_3D_EXT,
        GL_TEXTURE_CUBE_MAP_ARB,
        GL_TEXTURE_RECTANGLE_NV
    };
    const gchar * const target_names[] = {
        "1D",
        "2D",
        "3D",
        "cube-map",
        "rect"
    };
    guint trg;

    gldb_gui_combo_box_get_old(GTK_COMBO_BOX(pane->id), old,
                               COLUMN_TEXTURE_ID_ID, COLUMN_TEXTURE_ID_TARGET, -1);
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->id));
    gtk_list_store_clear(GTK_LIST_STORE(model));

    root = gldb_state_update();

    for (trg = 0; trg < G_N_ELEMENTS(targets); trg++)
    {
        s = gldb_state_find_child_enum(root, targets[trg]);
        if (!s) continue;
        for (nt = bugle_list_head(&s->children); nt != NULL; nt = bugle_list_next(nt))
        {
            t = (gldb_state *) bugle_list_data(nt);
            if (t->enum_name == 0)
            {
                /* Count the levels */
                f = t;
                if (targets[trg] == GL_TEXTURE_CUBE_MAP_ARB)
                    f = gldb_state_find_child_enum(t, GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB);
                levels = 0;
                channels = 0;
                for (nl = bugle_list_head(&f->children); nl != NULL; nl = bugle_list_next(nl))
                {
                    l = (gldb_state *) bugle_list_data(nl);
                    if (l->enum_name == 0)
                    {
                        int i;

                        levels = MAX(levels, (guint) l->numeric_name + 1);
                        for (i = 0; gldb_channel_table[i].channel; i++)
                            if (gldb_channel_table[i].texture_size_token
                                && (param = gldb_state_find_child_enum(l, gldb_channel_table[i].texture_size_token)) != NULL
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

    gldb_gui_combo_box_restore_old(GTK_COMBO_BOX(pane->id), old,
                                   COLUMN_TEXTURE_ID_ID, COLUMN_TEXTURE_ID_TARGET, -1);
}

static void gldb_texture_pane_id_changed(GtkComboBox *id_box, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint target, id, levels, channels;
    guint i, l;
    guint32 seq;
    GldbTexturePane *pane;
    texture_callback_data *data;

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
    {
        pane = GLDB_TEXTURE_PANE(user_data);

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->id));
        if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(pane->id), &iter)) return;
        gtk_tree_model_get(model, &iter,
                           COLUMN_TEXTURE_ID_ID, &id,
                           COLUMN_TEXTURE_ID_TARGET, &target,
                           COLUMN_TEXTURE_ID_LEVELS, &levels,
                           COLUMN_TEXTURE_ID_CHANNELS, &channels,
                           -1);

        for (l = 0; l < levels; l++)
        {
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
                    data->pane = pane;
                    if (l == 0 && i == 0)
                    {
                        data->flags |= TEXTURE_CALLBACK_FLAG_FIRST;
                        data->nlevels = levels;
                        data->nplanes = 6;
                        data->type = GLDB_GUI_IMAGE_TYPE_CUBE_MAP;
                    }
                    if (l == levels - 1 && i == 5) data->flags |= TEXTURE_CALLBACK_FLAG_LAST;
                    seq = gldb_gui_set_response_handler(gldb_texture_pane_response_callback, data);
                    gldb_send_data_texture(seq, id, target, data->face, l,
                                           gldb_channel_get_texture_token(channels),
                                           GL_FLOAT);
                }
            }
            else
            {
                data = (texture_callback_data *) bugle_malloc(sizeof(texture_callback_data));
                data->target = target;
                data->face = target;
                data->level = l;
                data->channels = channels;
                data->flags = 0;
                data->pane = pane;
                if (l == 0)
                {
                    data->flags |= TEXTURE_CALLBACK_FLAG_FIRST;
                    data->nlevels = levels;
                    data->nplanes = 1;
                    data->type = GLDB_GUI_IMAGE_TYPE_2D;
                    if (target == GL_TEXTURE_3D_EXT)
                    {
                        data->type = GLDB_GUI_IMAGE_TYPE_3D;
                        data->nplanes = 0;
                    }
                }
                if (l == levels - 1) data->flags |= TEXTURE_CALLBACK_FLAG_LAST;
                seq = gldb_gui_set_response_handler(gldb_texture_pane_response_callback, data);
                gldb_send_data_texture(seq, id, target, target, l,
                                       gldb_channel_get_texture_token(channels),
                                       GL_FLOAT);
            }
        }
    }
}

static GtkWidget *gldb_texture_pane_id_new(GldbTexturePane *pane)
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
                     G_CALLBACK(gldb_texture_pane_id_changed), pane);
    g_object_unref(G_OBJECT(store));
    return pane->id = id;
}

static GtkWidget *gldb_texture_pane_combo_table_new(GldbTexturePane *pane)
{
    GtkWidget *combos;
    GtkWidget *label, *id, *level, *face, *zoffset, *zoom, *min, *mag;

    combos = gtk_table_new(7, 2, FALSE);

    label = gtk_label_new(_("Texture"));
    id = gldb_texture_pane_id_new(pane);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 0, 1, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), id, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Level"));
    level = gldb_gui_image_viewer_level_new(pane->viewer);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 1, 2, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), level, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Face"));
    face = gldb_gui_image_viewer_face_new(pane->viewer);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 2, 3, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), face, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Z"));
    zoffset = gldb_gui_image_viewer_zoffset_new(pane->viewer);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 3, 4, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), zoffset, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Zoom"));
    zoom = pane->viewer->zoom;
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 4, 5, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), zoom, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Mag filter"));
    mag = gldb_gui_image_viewer_filter_new(pane->viewer, true);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 5, 6, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), mag, 1, 2, 5, 6, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    label = gtk_label_new(_("Min filter"));
    min = gldb_gui_image_viewer_filter_new(pane->viewer, false);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 6, 7, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), min, 1, 2, 6, 7, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    return combos;
}

static GtkWidget *gldb_texture_pane_toolbar_new(GldbTexturePane *pane)
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

GldbPane *gldb_texture_pane_new(GtkStatusbar *statusbar,
                                guint statusbar_context_id)
{
    GtkWidget *vbox, *hbox, *toolbar, *combos, *scrolled;
    GldbTexturePane *pane;

    pane = GLDB_TEXTURE_PANE(g_object_new(GLDB_TEXTURE_PANE_TYPE, NULL));
    pane->viewer = gldb_gui_image_viewer_new(statusbar, statusbar_context_id);
    combos = gldb_texture_pane_combo_table_new(pane);
    toolbar = gldb_texture_pane_toolbar_new(pane);

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

static void gldb_texture_pane_real_update(GldbPane *self)
{
    GldbTexturePane *pane;

    pane = GLDB_TEXTURE_PANE(self);
    /* Simply invoke a change event on the selector. This launches a
     * cascade of updates.
     */
    gldb_texture_pane_update_ids(pane);
}

/* GObject stuff */

static void gldb_texture_pane_class_init(GldbTexturePaneClass *klass)
{
    GldbPaneClass *pane_class = GLDB_PANE_CLASS(klass);

    pane_class->do_real_update = gldb_texture_pane_real_update;
}

static void gldb_texture_pane_init(GldbTexturePane *self, gpointer g_class)
{
    self->id = NULL;
    self->level = NULL;
    self->viewer = NULL;

    memset(&self->active, 0, sizeof(self->active));
    memset(&self->progressive, 0, sizeof(self->progressive));
}

GType gldb_texture_pane_get_type(void)
{
    static GType type = 0;
    if (type == 0)
    {
        static const GTypeInfo info =
        {
            sizeof(GldbTexturePaneClass),
            NULL,                       /* base_init */
            NULL,                       /* base_finalize */
            (GClassInitFunc) gldb_texture_pane_class_init,
            NULL,                       /* class_finalize */
            NULL,                       /* class_data */
            sizeof(GldbTexturePane),
            0,                          /* n_preallocs */
            (GInstanceInitFunc) gldb_texture_pane_init,
            NULL                        /* value table */
        };
        type = g_type_register_static(GLDB_PANE_TYPE,
                                      "GldbTexturePaneType",
                                      &info, 0);
    }
    return type;
}

#endif /* HAVE_GTKGLEXT */
