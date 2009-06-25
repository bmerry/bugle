/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009  Bruce Merry
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

#ifndef GETTEXT_PACKAGE
# define GETTEXT_PACKAGE "bugle00"
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include "gldb/gldb-common.h"
#include <budgie/macros.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include <bugle/string.h>
#include <bugle/memory.h>
#include "gldb/gldb-gui.h"
#include "gldb/gldb-gui-buffer.h"

struct _GldbBufferPane
{
    GldbPane parent;

    /* private */
    gboolean dirty;
    GtkWidget *top_widget;

    GtkWidget *id, *data_view, *format;
    GtkListStore *data_store;
    guint nfields;
    budgie_type *fields;

    void *data;
    guint32 length;
};

struct _GldbBufferPaneClass
{
    GldbPaneClass parent;
};

enum
{
    COLUMN_BUFFER_ID_ID,
    COLUMN_BUFFER_ID_TEXT
};

typedef struct
{
    GldbBufferPane *pane;
} buffer_callback_data;

/* Maps a letter to a column type and a GL type.
 * Returns true on success, false on illegal char.
 */
static gboolean parse_format_letter(gchar letter, GType *column, budgie_type *field)
{
    switch (letter)
    {
    case 'b': *column = G_TYPE_INT; *field = BUDGIE_TYPE_ID(6GLbyte); break;
    case 'B': *column = G_TYPE_INT; *field = BUDGIE_TYPE_ID(7GLubyte); break;
    case 's': *column = G_TYPE_INT; *field = BUDGIE_TYPE_ID(7GLshort); break;
    case 'S': *column = G_TYPE_INT; *field = BUDGIE_TYPE_ID(8GLushort); break;
    case 'i': *column = G_TYPE_INT; *field = BUDGIE_TYPE_ID(5GLint); break;
    case 'I': *column = G_TYPE_INT; *field = BUDGIE_TYPE_ID(6GLuint); break;
    case 'f': *column = G_TYPE_FLOAT; *field = BUDGIE_TYPE_ID(7GLfloat); break;
#if BUGLE_GLTYPE_GL
    case 'd': *column = G_TYPE_DOUBLE; *field = BUDGIE_TYPE_ID(8GLdouble); break;
#endif
    default: return FALSE;
    }
    return TRUE;
}

/* Parses a format string into both a set of columns to build a view, and
 * a set of fields (GL type tokens). NULL_TYPE is used to indicate pad bytes
 * in the fields, whereas nothing is added to the list of columns.
 * Returns true on success, false on an empty or badly formatted string, in
 * which case the outputs are unmodified.
 */
static gboolean parse_format(const gchar *format,
                             guint *ncolumns, GType **columns,
                             guint *nfields, budgie_type **fields)
{
    guint n = 0, f = 0, i;
    int number = -1;    /* when >= 0, a repeat count */
    const gchar *p;
    GType column;
    budgie_type field;

    /* First pass: count and validate */
    for (p = format; *p; p++)
    {
        if (*p == '_')
        {
            f += abs(number);
            number = -1;
        }
        else if (*p >= '0' && *p <= '9')
        {
            /* repeat count */
            if (number > 99)
            {
                /* Limit to repeat of 999. That's far more than the
                 * set of all enabled attributes can hold.
                 */
                return FALSE;
            }
            if (number == -1)
                number = 0;
            number = number * 10 + (*p - '0');
        }
        else if (parse_format_letter(*p, &column, &field))
        {
            n += abs(number);
            f += abs(number);
            number = -1;
        }
        else if (*p == ' ' || *p == '_')
        {
            /* Skip whitespace */
        }
        else
            return FALSE;
    }

    if (n == 0) return FALSE;   /* empty table */

    /* Second pass: allocation and filling in */
    *ncolumns = n;
    *nfields = f;
    *columns = BUGLE_NMALLOC(n, GType);
    *fields = BUGLE_NMALLOC(f, budgie_type);
    n = 0;
    f = 0;
    number = -1;

    p = format;
    for (p = format; *p; p++)
    {
        if (*p == '_')
        {
            for (i = 0; i < abs(number); i++)
                (*fields)[f++] = NULL_TYPE;
        }
        else if (*p >= '0' && *p <= '9')
        {
            /* repeat count */
            if (number > 99)
            {
                /* Limit to repeat of 999. That's far more than the
                 * set of all enabled attributes can hold.
                 */
                return FALSE;
            }
            if (number == -1)
                number = 0;
            number = number * 10 + (*p - '0');
        }
        else if (parse_format_letter(*p, &column, &field))
        {
            for (i = 0; i < abs(number); i++)
            {
                (*columns)[n++] = column;
                (*fields)[f++] = field;
            }
            number = -1;
        }
    }

    return TRUE;
}

static void gldb_buffer_pane_update_data(GldbBufferPane *pane)
{
    guint i;
    GtkTreeIter iter;
    const char *data;
    const char *end;
    gboolean valid;
    gboolean done = FALSE;
    gint column;

    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pane->data_store), &iter);

    data = (const char *) pane->data;
    end = data + pane->length;
    while (!done)
    {
        column = 0;
        /* Start the row - if it isn't completely filled, it will be culled later */
        if (!valid)
        {
            gtk_list_store_append(pane->data_store, &iter);
            valid = TRUE;
        }
        for (i = 0; i < pane->nfields && !done; i++)
        {
            if (pane->fields[i] != NULL_TYPE)
            {
                size_t size;
                GType column_type;

                /* Value might not be aligned - so make an aligned copy
                 * (double is in the union to force alignment).
                 */
                union
                {
                    double dummy;
                    char store[sizeof(double)];
                } aligned;
                int int_value;
                unsigned int uint_value;
                float float_value;
                double double_value;

                size = budgie_type_size(pane->fields[i]);
                if (data + size > end)
                    done = TRUE;
                else
                {
                    g_assert(size <= sizeof(double));
                    memcpy(&aligned.store, data, size);
                    data += size;
                    column_type = gtk_tree_model_get_column_type(GTK_TREE_MODEL(pane->data_store), column);
                    switch (column_type)
                    {
                    case G_TYPE_INT:
                        budgie_type_convert(&int_value, BUDGIE_TYPE_ID(i),
                                            &aligned.store, pane->fields[i], 1);
                        gtk_list_store_set(pane->data_store, &iter, column, (gint) int_value, -1);
                        break;
                    case G_TYPE_UINT:
                        budgie_type_convert(&uint_value, BUDGIE_TYPE_ID(j),
                                            &aligned.store, pane->fields[i], 1);
                        gtk_list_store_set(pane->data_store, &iter, column, (guint) uint_value, -1);
                        break;
                    case G_TYPE_FLOAT:
                        budgie_type_convert(&float_value, BUDGIE_TYPE_ID(f),
                                            &aligned.store, pane->fields[i], 1);
                        gtk_list_store_set(pane->data_store, &iter, column, (gfloat) float_value, -1);
                        break;
#if BUGLE_GLTYPE_GL
                    case G_TYPE_DOUBLE:
                        budgie_type_convert(&double_value, BUDGIE_TYPE_ID(d),
                                            &aligned.store, pane->fields[i], 1);
                        gtk_list_store_set(pane->data_store, &iter, column, (gdouble) double_value, -1);
                        break;
#endif
                    default:
                        g_assert_not_reached();
                    }
                    column++;
                }
            }
            else
            {
                /* Padding byte */
                if (data + 1 > end)
                    done = TRUE;
                else
                    data++;
            }
        }
        if (!done)
        {
            /* Move iterator to next row */
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(pane->data_store), &iter);
        }
    }

    /* Erase rows beyond the end of the update, and partially filled rows */
    while (valid)
    {
        valid = gtk_list_store_remove(pane->data_store, &iter);
    }
}

static gboolean gldb_buffer_pane_response_callback(gldb_response *response,
                                                   gpointer user_data)
{
    gldb_response_data_buffer *r;
    buffer_callback_data *data;

    r = (gldb_response_data_buffer *) response;
    data = (buffer_callback_data *) user_data;

    free(data->pane->data);
    data->pane->data = r->data;
    data->pane->length = r->length;
    r->data = NULL; /* prevents gldb_free_response from freeing it */

    gldb_buffer_pane_update_data(data->pane);

    gldb_free_response(response);
    free(data);
    return TRUE;
}

static void gldb_buffer_pane_update_ids(GldbBufferPane *pane)
{
    GValue old[1];
    gldb_state *root, *s;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *name;
    linked_list_node *i;

    root = gldb_state_update();
    g_return_if_fail(root != NULL);

    gldb_gui_combo_box_get_old(GTK_COMBO_BOX(pane->id), old,
                               COLUMN_BUFFER_ID_ID, -1);

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->id));
    gtk_list_store_clear(GTK_LIST_STORE(model));
    for (i = bugle_list_head(&root->children); i != NULL; i = bugle_list_next(i))
    {
        s = (gldb_state *) bugle_list_data(i);
        if (gldb_state_find_child_enum(s, GL_BUFFER_SIZE) != NULL)
        {
            name = bugle_asprintf("%u", (unsigned int) s->numeric_name);
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COLUMN_BUFFER_ID_ID, (guint) s->numeric_name,
                               COLUMN_BUFFER_ID_TEXT, name,
                               -1);
            free(name);
        }
    }

    gldb_gui_combo_box_restore_old(GTK_COMBO_BOX(pane->id), old,
                                   COLUMN_BUFFER_ID_ID, -1);
}

static void gldb_buffer_pane_id_changed(GtkComboBox *id_box, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint id;
    guint32 seq;
    GldbBufferPane *pane;
    buffer_callback_data *data;

    if (gldb_get_status() == GLDB_STATUS_STOPPED)
    {
        pane = GLDB_BUFFER_PANE(user_data);

        model = gtk_combo_box_get_model(GTK_COMBO_BOX(pane->id));
        if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(pane->id), &iter)) return;
        gtk_tree_model_get(model, &iter,
                           COLUMN_BUFFER_ID_ID, &id,
                           -1);

        data = BUGLE_MALLOC(buffer_callback_data);
        data->pane = pane;
        seq = gldb_gui_set_response_handler(gldb_buffer_pane_response_callback, data);
        gldb_send_data_buffer(seq, id);
    }
}

GtkWidget *gldb_buffer_pane_id_new(GldbBufferPane *pane)
{
    GtkListStore *store;
    GtkWidget *id;
    GtkCellRenderer *cell;

    store = gtk_list_store_new(2,
                               G_TYPE_UINT,  /* ID */
                               G_TYPE_STRING);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
                                         COLUMN_BUFFER_ID_ID,
                                         GTK_SORT_ASCENDING);
    id = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_widget_set_size_request(id, 80, -1);

    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(id), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(id), cell,
                                   "text", COLUMN_BUFFER_ID_TEXT,
                                   NULL);
    g_signal_connect(G_OBJECT(id), "changed",
                     G_CALLBACK(gldb_buffer_pane_id_changed), pane);
    g_object_unref(G_OBJECT(store));
    return pane->id = id;
}

GtkWidget *gldb_buffer_pane_combo_table_new(GldbBufferPane *pane)
{
    GtkWidget *combos;
    GtkWidget *label, *id;

    combos = gtk_table_new(1, 2, FALSE);

    label = gtk_label_new(_("Buffer"));
    id = gldb_buffer_pane_id_new(pane);
    gtk_table_attach(GTK_TABLE(combos), label, 0, 1, 0, 1, 0, 0, 0, 0);
    gtk_table_attach(GTK_TABLE(combos), id, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    return combos;
}

/* Constructs the store and the view from the format description */
static GtkWidget *gldb_buffer_pane_rebuild_view(GtkWidget *editable, GldbBufferPane *pane)
{
    guint ncolumns, nfields, i;
    GType *columns;
    budgie_type *fields;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    gchar *format;

    /* Get the entire format string */
    format = gtk_editable_get_chars(GTK_EDITABLE(editable), 0, -1);
    if (!parse_format(format, &ncolumns, &columns, &nfields, &fields))
    {
        free(format);
        /* Invalid or empty format; leave as is until it is valid again */
        return pane->data_view;
    }

    /* Old store is ref-counted by the view, so it will go once we set the model */
    pane->data_store = gtk_list_store_newv(ncolumns, columns);

    if (pane->data_view != NULL)
    {
        /* Already exists - cull the existing columns */
        GtkTreeViewColumn *column;

        while ((column = gtk_tree_view_get_column(GTK_TREE_VIEW(pane->data_view), 0)) != NULL)
            gtk_tree_view_remove_column(GTK_TREE_VIEW(pane->data_view), column);
        gtk_tree_view_set_model(GTK_TREE_VIEW(pane->data_view), GTK_TREE_MODEL(pane->data_store));
    }
    else
    {
        /* Does not yet exist, so create it */
        pane->data_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pane->data_store));
    }

    for (i = 0; i < ncolumns; i++)
    {
        cell = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("",
                                                          cell,
                                                          "text", i,
                                                          NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(pane->data_view), column);
    }
    free(columns);
    free(format);
    free(pane->fields);
    pane->fields = fields;
    pane->nfields = nfields;

    gldb_buffer_pane_update_data(pane);

    /* Make sure store dies with the view */
    g_object_unref(pane->data_store);
    return pane->data_view;
}

GtkWidget *gldb_buffer_pane_format_new(GldbBufferPane *pane)
{
    pane->format = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(pane->format), "B");
    g_signal_connect(G_OBJECT(pane->format), "changed",
                     G_CALLBACK(gldb_buffer_pane_rebuild_view), pane);
    return pane->format;
}

GldbPane *gldb_buffer_pane_new(void)
{
    GtkWidget *hbox, *vbox, *scrolled, *combos, *format, *help;
    GldbBufferPane *pane;

    pane = GLDB_BUFFER_PANE(g_object_new(GLDB_BUFFER_PANE_TYPE, NULL));

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    combos = gldb_buffer_pane_combo_table_new(pane);
    format = gldb_buffer_pane_format_new(pane);
    help = gtk_label_new(_("b = byte\n"
                           "B = ubyte\n"
                           "s = short\n"
                           "S = ushort\n"
                           "i = int\n"
                           "I = uint\n"
                           "f = float\n"
#ifdef BUGLE_GLTYPE_GL
                           "d = double\n"
#endif
                           "_ = pad byte\n"
                           "3x = xxx"));
    gldb_buffer_pane_rebuild_view(format, pane);
    gtk_container_add(GTK_CONTAINER(scrolled), pane->data_view);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), combos, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), format, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), help, FALSE, FALSE, 0);
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);
    gldb_pane_set_widget(GLDB_PANE(pane), hbox);
    return GLDB_PANE(pane);
}

static void gldb_buffer_pane_real_update(GldbPane *self)
{
    GldbBufferPane *pane;

    pane = GLDB_BUFFER_PANE(self);
    gldb_buffer_pane_update_ids(pane);
}

/* GObject stuff */

static void gldb_buffer_pane_finalize(GldbBufferPane *pane)
{
    free(pane->data);
    free(pane->fields);
}

static void gldb_buffer_pane_class_init(GldbBufferPaneClass *klass)
{
    GldbPaneClass *pane_class;

    pane_class = GLDB_PANE_CLASS(klass);
    pane_class->do_real_update = gldb_buffer_pane_real_update;
    G_OBJECT_CLASS(pane_class)->finalize = (GObjectFinalizeFunc) gldb_buffer_pane_finalize;
}

static void gldb_buffer_pane_init(GldbBufferPane *self, gpointer g_class)
{
    self->data = NULL;
    self->data_view = NULL;
    self->data_store = NULL;
    self->fields = NULL;
    self->nfields = 0;
}

GType gldb_buffer_pane_get_type(void)
{
    static GType type = 0;
    if (type == 0)
    {
        static const GTypeInfo info =
        {
            sizeof(GldbBufferPaneClass),
            NULL,                       /* base_init */
            NULL,                       /* base_finalize */
            (GClassInitFunc) gldb_buffer_pane_class_init,
            NULL,                       /* class_finalize */
            NULL,                       /* class_data */
            sizeof(GldbBufferPane),
            0,                          /* n_preallocs */
            (GInstanceInitFunc) gldb_buffer_pane_init,
            NULL                        /* value table */
        };
        type = g_type_register_static(GLDB_PANE_TYPE,
                                      "GldbBufferPaneType",
                                      &info, 0);
    }
    return type;
}

