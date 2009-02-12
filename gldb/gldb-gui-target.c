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
#include <stdlib.h>
#include <string.h>
#include "gldb-common.h"

#define COLUMN_MODE_NAME 0
#define COLUMN_MODE_ENUM 1

typedef struct GldbGuiTargetDialog
{
    GtkWidget *dialog;
    GtkWidget *mode;

    GtkWidget *settings[GLDB_PROGRAM_SETTING_COUNT];
    GtkWidget *labels[GLDB_PROGRAM_SETTING_COUNT];
} GldbGuiTargetDialog;

static GtkTreeModel *target_model(void)
{
    GtkListStore *store;
    GtkTreeIter iter;

    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_MODE_NAME, _("Local"),
                       COLUMN_MODE_ENUM, GLDB_PROGRAM_TYPE_LOCAL,
                       -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_MODE_NAME, _("Remote via SSH"),
                       COLUMN_MODE_ENUM, GLDB_PROGRAM_TYPE_SSH,
                       -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_MODE_NAME, _("Remote via TCP/IP"),
                       COLUMN_MODE_ENUM, GLDB_PROGRAM_TYPE_TCP,
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
    gldb_program_setting setting;

    context = (GldbGuiTargetDialog *) user_data;
    model = gtk_combo_box_get_model(mode);
    gtk_combo_box_get_active_iter(mode, &iter);
    gtk_tree_model_get(model, &iter,
                       COLUMN_MODE_ENUM, &mode_enum,
                       -1);

    for (setting = 0; setting < GLDB_PROGRAM_SETTING_COUNT; setting++)
    {
        gboolean visible;
        visible = gldb_program_type_has_setting(mode_enum, setting);
        show_hide(context->settings[setting], visible);
        show_hide(context->labels[setting], visible);
    }
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

    g_signal_connect(G_OBJECT(mode), "changed",
                     G_CALLBACK(target_mode_changed), context);
    return context->mode = mode;
}

/* Adds a label for a widget and the widget. The label widget is returned */
static GtkWidget *add_labelled(GtkWidget *table, int row,
                               const char *text, GtkWidget *right)
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
    gtk_widget_show(label);
    gtk_widget_show(right);
    return label;
}

static void target_update(GldbGuiTargetDialog *context)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint mode_enum;
    gldb_program_setting setting;
    const char *value;

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(context->mode));
    gtk_combo_box_get_active_iter(GTK_COMBO_BOX(context->mode), &iter);
    gtk_tree_model_get(model, &iter,
                       COLUMN_MODE_ENUM, &mode_enum,
                       -1);

    gldb_program_set_type(mode_enum);
    for (setting = 0; setting < GLDB_PROGRAM_SETTING_COUNT; setting++)
    {
        if (gldb_program_type_has_setting(mode_enum, setting))
        {
            value = (const char *) gtk_entry_get_text(GTK_ENTRY(context->settings[setting]));
            gldb_program_set_setting(setting, value);
        }
    }
}

static GtkWidget *entry_new_with_setting(gldb_program_setting setting)
{
    GtkWidget *entry;
    const char *text;

    entry = gtk_entry_new();
    text = gldb_program_get_setting(setting);
    if (text)
        gtk_entry_set_text(GTK_ENTRY(entry), (const gchar *) text);
    return entry;
}

void gldb_gui_target_dialog_run(GtkWindow *parent)
{
    GldbGuiTargetDialog context;
    gint result;
    GtkWidget *table;
    gldb_program_setting setting;
    static const gchar *names[GLDB_PROGRAM_SETTING_COUNT] =
    {
        "Executable",
        "Filter chain",
        "X11 Display",
        "Host",
        "Port"
    };

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
    for (setting = 0; setting < GLDB_PROGRAM_SETTING_COUNT; setting++)

    table = gtk_table_new(GLDB_PROGRAM_SETTING_COUNT + 1, 2, FALSE);
    add_labelled(table, 0, _("Mode"), context.mode);
    for (setting = 0; setting < GLDB_PROGRAM_SETTING_COUNT; setting++)
    {
        context.settings[setting] = entry_new_with_setting(setting);
        context.labels[setting] = add_labelled(table, setting + 1,
                                               _(names[setting]), context.settings[setting]);
    }
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
                       table, TRUE, FALSE, 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(context.mode), gldb_program_get_type());
    gtk_widget_show(table);

    result = gtk_dialog_run(GTK_DIALOG(context.dialog));
    if (result == GTK_RESPONSE_ACCEPT)
        target_update(&context);
    gtk_widget_destroy(context.dialog);
}
