/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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

/* Shader pane. */

#if HAVE_CONFIG_H
# include <config.h>
#endif
/* FIXME: Not sure if this is the correct definition of GETTEXT_PACKAGE... */
#ifndef GETTEXT_PACKAGE
# define GETTEXT_PACKAGE "bugle00"
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <budgie/reflect.h>
#include "gldb/gldb-gui.h"
#include "gldb/gldb-gui-breakpoint.h"
#include "gldb/gldb-common.h"

struct _GldbBreakpointPane
{
    GldbPane parent;

    /* private */
    GtkListStore *breakpoint_store;
    GtkListStore *function_store;
    GtkWidget *list;
};

struct _GldbBreakpointPaneClass
{
    GldbPaneClass parent;
};

enum
{
    COLUMN_BREAKPOINT_ENABLED,
    COLUMN_BREAKPOINT_FUNCTION
};

static void gldb_breakpoint_pane_add(GtkButton *button, gpointer user_data)
{
    GldbBreakpointPane *pane;
    GtkWidget *dialog, *entry;
    GtkEntryCompletion *completion;
    gint result;

    pane = GLDB_BREAKPOINT_PANE(user_data);

    dialog = gtk_dialog_new_with_buttons(_("Add breakpoint"),
                                         GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    entry = gtk_entry_new();
    /* Protect against earlier dispose */
    if (pane->function_store)
    {
        completion = gtk_entry_completion_new();
        gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(pane->function_store));
        gtk_entry_completion_set_minimum_key_length(completion, 4);
        gtk_entry_completion_set_text_column(completion, 0);
        gtk_entry_set_completion(GTK_ENTRY(entry), completion);
    }
    gtk_widget_show(entry);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), entry, TRUE, FALSE, 0);

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT)
    {
        GtkTreeIter iter;

        gtk_list_store_append(pane->breakpoint_store, &iter);
        gtk_list_store_set(pane->breakpoint_store, &iter,
                           COLUMN_BREAKPOINT_ENABLED, TRUE,
                           COLUMN_BREAKPOINT_FUNCTION, gtk_entry_get_text(GTK_ENTRY(entry)),
                           -1);
        gldb_set_break(0, gtk_entry_get_text(GTK_ENTRY(entry)), true);
    }
    gtk_widget_destroy(dialog);
}

static void gldb_breakpoint_pane_remove(GtkButton *button, gpointer user_data)
{
    GldbBreakpointPane *pane;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *model;
    const gchar *func;
    gboolean enabled;

    pane = GLDB_BREAKPOINT_PANE(user_data);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pane->list));
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_tree_model_get(model, &iter,
                           COLUMN_BREAKPOINT_ENABLED, &enabled,
                           COLUMN_BREAKPOINT_FUNCTION, &func,
                           -1);
        if (enabled)
            gldb_set_break(0, func, false);
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    }
}

static void gldb_breakpoint_pane_error_toggled(GtkWidget *toggle, gpointer user_data)
{
    GldbBreakpointPane *pane;

    pane = GLDB_BREAKPOINT_PANE(user_data);
    gldb_set_break_error(0, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));
}

static void gldb_breakpoint_pane_cell_toggled(GtkCellRendererToggle *cell,
                                              gchar *path_string,
                                              gpointer user_data)
{
    GldbBreakpointPane *pane;
    GtkTreeIter iter;
    gboolean enabled;
    gchar *function;

    pane = GLDB_BREAKPOINT_PANE(user_data);
    if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(pane->breakpoint_store), &iter, path_string))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(pane->breakpoint_store), &iter,
                           COLUMN_BREAKPOINT_ENABLED, &enabled,
                           COLUMN_BREAKPOINT_FUNCTION, &function,
                           -1);
        enabled = !enabled;
        gtk_list_store_set(pane->breakpoint_store, &iter, COLUMN_BREAKPOINT_ENABLED, enabled, -1);
        gldb_set_break(0, function, enabled);
        g_free(function);
    }
}

GldbPane *gldb_breakpoint_pane_new(void)
{
    GldbBreakpointPane *pane;
    GtkWidget *hbox, *vbox, *bbox, *btn, *toggle;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    int i;
    GtkTreeIter iter;

    pane = GLDB_BREAKPOINT_PANE(g_object_new(GLDB_BREAKPOINT_PANE_TYPE, NULL));
    pane->breakpoint_store = gtk_list_store_new(2, G_TYPE_BOOLEAN, G_TYPE_STRING);
    pane->function_store = gtk_list_store_new(1, G_TYPE_STRING);
    for (i = 0; i < budgie_function_count(); i++)
    {
        gtk_list_store_append(pane->function_store, &iter);
        gtk_list_store_set(pane->function_store, &iter, 0, budgie_function_name(i), -1);
    }

    hbox = gtk_hbox_new(FALSE, 0);
    vbox = gtk_vbox_new(FALSE, 0);

    pane->list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pane->breakpoint_store));
    cell = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes(_("Enabled"),
                                                      cell,
                                                      "active", COLUMN_BREAKPOINT_ENABLED,
                                                      NULL);
    g_signal_connect(G_OBJECT(cell), "toggled",
                     G_CALLBACK(gldb_breakpoint_pane_cell_toggled), pane);
    gtk_tree_view_append_column(GTK_TREE_VIEW(pane->list), column);
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Function"),
                                                      cell,
                                                      "text", COLUMN_BREAKPOINT_FUNCTION,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(pane->list), column);
    g_object_unref(pane->breakpoint_store);

    gtk_box_pack_start(GTK_BOX(hbox), pane->list, TRUE, TRUE, 0);

    bbox = gtk_vbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
    btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
    g_signal_connect(G_OBJECT(btn), "clicked",
                     G_CALLBACK(gldb_breakpoint_pane_add), pane);
    gtk_box_pack_start(GTK_BOX(bbox), btn, TRUE, FALSE, 0);
    btn = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    g_signal_connect(G_OBJECT(btn), "clicked",
                     G_CALLBACK(gldb_breakpoint_pane_remove), pane);
    gtk_box_pack_start(GTK_BOX(bbox), btn, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), bbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    toggle = gtk_check_button_new_with_mnemonic(_("Break on _errors"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), gldb_get_break_error());
    g_signal_connect(G_OBJECT(toggle), "toggled",
                     G_CALLBACK(gldb_breakpoint_pane_error_toggled), pane);
    gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);

    gldb_pane_set_widget(GLDB_PANE(pane), vbox);
    return GLDB_PANE(pane);
}

static void gldb_breakpoint_pane_real_update(GldbPane *self)
{
    /* Nothing needs to be done */
}

static void gldb_breakpoint_pane_dispose(GObject *self)
{
    GldbBreakpointPane *pane;
    GldbBreakpointPaneClass *klass;
    GObjectClass *parent_class;

    pane = GLDB_BREAKPOINT_PANE(self);
    g_object_unref(pane->function_store);
    pane->function_store = NULL;

    /* Chain up */
    klass = GLDB_BREAKPOINT_PANE_GET_CLASS(self);
    parent_class = G_OBJECT_CLASS(g_type_class_peek_parent(klass));
    parent_class->dispose(self);
}

/* GObject stuff */

static void gldb_breakpoint_pane_class_init(GldbBreakpointPaneClass *klass)
{
    GldbPaneClass *pane_class;
    GObjectClass *object_class;

    pane_class = GLDB_PANE_CLASS(klass);
    pane_class->do_real_update = gldb_breakpoint_pane_real_update;

    object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = gldb_breakpoint_pane_dispose;
}

static void gldb_breakpoint_pane_init(GldbBreakpointPane *self, gpointer g_class)
{
    self->breakpoint_store = NULL;
    self->function_store = NULL;
    self->list = NULL;
}

GType gldb_breakpoint_pane_get_type(void)
{
    static GType type = 0;
    if (type == 0)
    {
        static const GTypeInfo info =
        {
            sizeof(GldbBreakpointPaneClass),
            NULL,                       /* base_init */
            NULL,                       /* base_finalize */
            (GClassInitFunc) gldb_breakpoint_pane_class_init,
            NULL,                       /* class_finalize */
            NULL,                       /* class_data */
            sizeof(GldbBreakpointPane),
            0,                          /* n_preallocs */
            (GInstanceInitFunc) gldb_breakpoint_pane_init,
            NULL                        /* value table */
        };
        type = g_type_register_static(GLDB_PANE_TYPE,
                                      "GldbBreakpointPaneType",
                                      &info, 0);
    }
    return type;
}
