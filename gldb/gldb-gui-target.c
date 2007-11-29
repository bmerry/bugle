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
#include "gldb-common.h"

#define COLUMN_MODE_NAME 0
#define COLUMN_MODE_ENUM 1

typedef struct
{
    GtkWidget *dialog;
    GtkWidget *mode, *command;

    GtkWidget *remote_host_label, *remote_host;
} GldbGuiTargetDialog;

static GtkTreeModel *target_model(void)
{
    GtkListStore *store;
    GtkTreeIter iter;

    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_MODE_NAME, _("Local"),
                       COLUMN_MODE_ENUM, 0,
                       -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_MODE_NAME, _("Remote"),
                       COLUMN_MODE_ENUM, 1,
                       -1);
    return GTK_TREE_MODEL(store);
}

static void show_hide(GtkWidget *widget, gboolean visible)
{
    if (visible) gtk_widget_show(widget);
    else gtk_widget_hide(widget);
}

static void target_mode_changed(GtkComboBox *mode, gpointer user_data)
{
    GldbGuiTargetDialog *context;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint mode_enum;
    gboolean remote_visible;

    context = (GldbGuiTargetDialog *) user_data;
    model = gtk_combo_box_get_model(mode);
    gtk_combo_box_get_active_iter(mode, &iter);
    gtk_tree_model_get(model, &iter,
                       COLUMN_MODE_ENUM, &mode_enum,
                       -1);

    remote_visible = (mode_enum == 1);
    show_hide(context->remote_host_label, remote_visible);
    show_hide(context->remote_host, remote_visible);
}

static GtkWidget *target_mode(GldbGuiTargetDialog *context)
{
    GtkTreeModel *model;
    GtkWidget *mode;
    GtkCellRenderer *cell;

    model = target_model();
    mode = gtk_combo_box_new_with_model(model);
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(mode), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(mode), cell,
                                   "text", COLUMN_MODE_NAME,
                                   NULL);
    g_object_unref(model);

    gtk_combo_box_set_active(GTK_COMBO_BOX(mode), 0);
    g_signal_connect(G_OBJECT(mode), "changed",
                     G_CALLBACK(target_mode_changed), context);
    return context->mode = mode;
}

/* Adds a label for a widget and the widget. The label widget is returned */
static GtkWidget *add_labelled(GtkWidget *table, int row,
                               const char *text, GtkWidget *right,
                               gboolean visible)
{
    GtkWidget *label;
    gfloat xalign, yalign;

    label = gtk_label_new(text);
    gtk_misc_get_alignment(GTK_MISC(label), &xalign, &yalign);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, yalign);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row + 1, GTK_FILL, 0, 5, 2);
    gtk_table_attach(GTK_TABLE(table), right,
                     1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, 0, 5, 2);
    if (visible)
    {
        gtk_widget_show(label);
        gtk_widget_show(right);
    }
    return label;
}

void gldb_gui_target_update(GldbGuiTargetDialog *context)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint mode_enum;
    const char *command, *remote_host;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->mode));
    gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->mode), &iter);
    gtk_tree_model_get(model, &iter,
                       COLUMN_MODE_ENUM, &mode_enum,
                       -1);
    switch (mode_enum)
    {
    case 0: /* Local */
        command = (const char *) gtk_entry_get_text(GTK_ENTRY(context->command));
        gldb_program_set_local(1, &command);
        /* FIXME: dialog should take separated arguments */
        break;
    case 1: /* Remote */
        command = (const char *) gtk_entry_get_text(GTK_ENTRY(context->command));
        remote_host = (const char *) gtk_entry_get_text(GTK_ENTRY(context->remote_host));
        gldb_program_set_remote(command, remote_host, ":0.0");
        break;
    }
}

void gldb_gui_do_target_dialog(GtkWindow *parent)
{
    GldbGuiTargetDialog context;
    gint result;
    GtkWidget *table;

    context.dialog = gtk_dialog_new_with_buttons(_("Target"),
                                                 parent,
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                                 NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(context.dialog),
                                    GTK_RESPONSE_ACCEPT);
    gtk_window_set_resizable(GTK_WINDOW(context.dialog), FALSE);

    target_mode(&context);
    context.command = gtk_entry_new();
    context.remote_host = gtk_entry_new();

    table = gtk_table_new(3, 2, FALSE);
    add_labelled(table, 0, _("Mode"), context.mode, TRUE);
    add_labelled(table, 1, _("Executable"), context.command, TRUE);
    context.remote_host_label = add_labelled(table, 2, _("Host"), context.remote_host, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
                       table, TRUE, FALSE, 0);
    gtk_widget_show(table);

    result = gtk_dialog_run(GTK_DIALOG(context.dialog));
    if (result == GTK_RESPONSE_ACCEPT)
        gldb_gui_target_update(&context);
    gtk_widget_destroy(context.dialog);
}
