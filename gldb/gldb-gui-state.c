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

/* State pane. Note that non-GUI state handling is in gldb-common.c */

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
#include <string.h>
#include <stdlib.h>
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include "common/bool.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-gui.h"
#include "gldb/gldb-gui-state.h"

struct _GldbStatePane
{
    GldbPane parent;

    /* private */
    gboolean dirty;
    GtkWidget *top_widget;
    GtkWidget *only_selected, *only_modified;
    GtkWidget *tree_view;
    GtkTreeStore *state_store;
    GtkTreeModel *state_filter;  /* Filter that shows only chosen state */
    GtkTreeModel *state_sort;    /* GtkTreeModelSort over the filter */
};

struct _GldbStatePaneClass
{
    GldbPaneClass parent;
};

enum
{
    COLUMN_STATE_NAME,
    COLUMN_STATE_VALUE,
    COLUMN_STATE_SELECTED,
    COLUMN_STATE_MODIFIED,
    COLUMN_STATE_SELECTED_TOTAL,
    COLUMN_STATE_MODIFIED_TOTAL,
    COLUMN_STATE_BOLD,
    COLUMN_STATE_EXPANDED
};

static void dump_state_xml_r(const gldb_state *root, GString *f, gboolean top)
{
    gchar *header;
    gchar *name_utf8, *value_utf8;
    char *value;
    bugle_list_node *i;
    const gldb_state *child;

    if (top)
        g_string_append(f, "<state-tree>");
    else
    {
        value = gldb_state_string(root);
        value_utf8 = g_convert(value, -1, "UTF8", "ASCII", NULL, NULL, NULL);
        name_utf8 = g_convert(root->name, -1, "UTF8", "ASCII", NULL, NULL, NULL);
        header = g_markup_printf_escaped("<state name=\"%s\">%s",
                                         name_utf8, value_utf8);
        g_string_append(f, header);
        free(value);
        g_free(name_utf8);
        g_free(value_utf8);
        g_free(header);
    }

    for (i = bugle_list_head(&root->children); i; i = bugle_list_next(i))
    {
        child = (const gldb_state *) bugle_list_data(i);
        dump_state_xml_r(child, f, FALSE);
    }

    g_string_append(f, top ? "</state-tree>" : "</state>");
}

static gchar *dump_state_xml(const gldb_state *root)
{
    GString *f;
    gchar *data;

    f = g_string_new("<?xml version=\"1.0\"?>");
    dump_state_xml_r(root, f, TRUE);
    data = f->str;
    g_string_free(f, FALSE);
    return data;
}

/* Utility functions for dealing with columns that hold totalling
 * information about sub-trees.
 */

/* Adds X to column of iter and all its ancestors, from the top down. */
static void add_total_column(GtkTreeStore *store, GtkTreeIter *iter,
                             gint column, gint value)
{
    gint old;

    if (iter)
    {
        GtkTreeIter parent;
        if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(store), &parent, iter))
            add_total_column(store, &parent, column, value);
    }
    gtk_tree_model_get(GTK_TREE_MODEL(store), iter, column, &old, -1);
    gtk_tree_store_set(store, iter, column, old + value, -1);
}

/* Sets base_column (a boolean) to value, and updates the count in
 * total_column.
 */
static void set_column(GtkTreeStore *store, GtkTreeIter *iter,
                       gint base_column, gint total_column, gboolean value)
{
    gboolean old;

    gtk_tree_model_get(GTK_TREE_MODEL(store), iter, base_column, &old, -1);
    if (old != value)
        gtk_tree_store_set(store, iter, base_column, value, -1);
    if (old && !value)
        add_total_column(store, iter, total_column, -1);
    else if (!old && value)
        add_total_column(store, iter, total_column, 1);
}

/* Removes iter and all descendants from the tree, keeping the total
 * counts intact. It modifies iter to point to the next child as for
 * gtk_tree_store_remove, and returns the same boolean.
 */
static gboolean state_remove_r(GtkTreeStore *store, GtkTreeIter *iter)
{
    GtkTreeIter child;
    gboolean valid;

    valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &child, iter);
    while (valid)
        valid = state_remove_r(store, &child);
    set_column(store, iter, COLUMN_STATE_SELECTED, COLUMN_STATE_SELECTED_TOTAL, FALSE);
    set_column(store, iter, COLUMN_STATE_MODIFIED, COLUMN_STATE_MODIFIED_TOTAL, FALSE);
    return gtk_tree_store_remove(store, iter);
}

/* We can't just rip out all the old state and plug in the new, because
 * that loses any expansions and selections that may have been active.
 * Instead, we take each state in the store and try to match it with state
 * in the gldb state tree. If it fails, we delete the state. Any new state
 * also has to be placed in the correct position.
 *
 * As an added bonus, this approach makes it really easy to highlight
 * state that has changed.
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
        if (child->name)
        {
            if (bugle_hash_get(&lookup, child->name))
                g_warning("State %s is duplicated", child->name);
            bugle_hash_set(&lookup, child->name, (void *) child);
        }
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
            char *value;
            gchar *value_utf8, *old_utf8;
            gboolean changed;

            value = gldb_state_string(child);
            value_utf8 = g_convert(value, -1, "UTF8", "ASCII", NULL, NULL, NULL);
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                               COLUMN_STATE_VALUE, &old_utf8,
                               -1);
            changed = strcmp(old_utf8, value_utf8) != 0;
            if (changed)
            {
                gtk_tree_store_set(store, &iter,
                                   COLUMN_STATE_VALUE, value_utf8,
                                   COLUMN_STATE_BOLD, PANGO_WEIGHT_BOLD,
                                   -1);
                set_column(store, &iter, COLUMN_STATE_MODIFIED, COLUMN_STATE_MODIFIED_TOTAL, TRUE);
            }
            else
            {
                gtk_tree_store_set(store, &iter,
                                   COLUMN_STATE_BOLD, PANGO_WEIGHT_NORMAL,
                                   -1);
                set_column(store, &iter, COLUMN_STATE_MODIFIED, COLUMN_STATE_MODIFIED_TOTAL, FALSE);
            }
            free(value);
            g_free(old_utf8);
            g_free(value_utf8);
            update_state_r(child, store, &iter);
            /* Mark as seen for the next phase */
            bugle_hash_set(&lookup, child->name, NULL);
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        }
        else
            valid = state_remove_r(store, &iter);
    }

    /* Fill in missing items */
    valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &iter, parent);
    for (i = bugle_list_head(&root->children); i; i = bugle_list_next(i))
    {
        child = (const gldb_state *) bugle_list_data(i);

        if (!child->name) continue;
        /* The hash is cleared for items that have been seen; thus "if new" */
        if (bugle_hash_get(&lookup, child->name))
        {
            char *value;
            gchar *value_utf8;

            value = gldb_state_string(child);
            value_utf8 = g_convert(value, -1, "UTF8", "ASCII", NULL, NULL, NULL);
            gtk_tree_store_insert_before(store, &iter2, parent,
                                         valid ? &iter : NULL);
            gtk_tree_store_set(store, &iter2,
                               COLUMN_STATE_NAME, child->name,
                               COLUMN_STATE_VALUE, value_utf8 ? value_utf8 : "",
                               COLUMN_STATE_BOLD, PANGO_WEIGHT_BOLD,
                               COLUMN_STATE_MODIFIED, FALSE,  /* will update */
                               COLUMN_STATE_SELECTED, FALSE,
                               COLUMN_STATE_EXPANDED, FALSE,
                               -1);
            set_column(store, &iter2, COLUMN_STATE_MODIFIED, COLUMN_STATE_MODIFIED_TOTAL, TRUE);
            free(value);
            g_free(value_utf8);
            update_state_r(child, store, &iter2);
        }
        else if (valid)
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }

    bugle_hash_clear(&lookup);
}

static void state_select_toggled(GtkCellRendererToggle *cell,
                                 gchar *path,
                                 gpointer user_data)
{
    GldbStatePane *pane;
    GtkTreeStore *store;
    GtkTreeModelFilter *filter;
    GtkTreeModelSort *sort;
    GtkTreeIter sort_iter, filter_iter, store_iter;
    gboolean selected;

    pane = GLDB_STATE_PANE(user_data);
    sort = GTK_TREE_MODEL_SORT(pane->state_sort);
    filter = GTK_TREE_MODEL_FILTER(pane->state_filter);
    store = GTK_TREE_STORE(pane->state_store);
    if (gtk_tree_model_get_iter_from_string(pane->state_sort, &sort_iter, path))
    {
        gtk_tree_model_sort_convert_iter_to_child_iter(sort, &filter_iter, &sort_iter);
        gtk_tree_model_filter_convert_iter_to_child_iter(filter, &store_iter, &filter_iter);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &store_iter,
                           COLUMN_STATE_SELECTED, &selected, -1);
        set_column(store, &store_iter, COLUMN_STATE_SELECTED, COLUMN_STATE_SELECTED_TOTAL, !selected);
    }
}

static void state_expand_r(GtkTreeView *view, GtkTreeModel *model, GtkTreeIter *root)
{
    GtkTreePath *path;
    GtkTreeIter child;
    gboolean valid;

    if (root)
    {
        gboolean expanded;

        gtk_tree_model_get(model, root, COLUMN_STATE_EXPANDED, &expanded, -1);
        if (expanded)
        {
            path = gtk_tree_model_get_path(model, root);
            gtk_tree_view_expand_row(view, path, FALSE);
            gtk_tree_path_free(path);
        }
    }

    valid = gtk_tree_model_iter_children(model, &child, root);
    while (valid)
    {
        state_expand_r(view, model, &child);
        valid = gtk_tree_model_iter_next(model, &child);
    }
}

static void state_filter_toggled(GtkWidget *widget,
                                 gpointer user_data)
{
    GldbStatePane *pane;
    pane = GLDB_STATE_PANE(user_data);
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(pane->state_filter));
    state_expand_r(GTK_TREE_VIEW(pane->tree_view),
                   gtk_tree_view_get_model(GTK_TREE_VIEW(pane->tree_view)),
                   NULL);
}

static gboolean state_visible(GtkTreeModel *model, GtkTreeIter *iter,
                              gpointer user_data)
{
    GldbStatePane *pane;
    gboolean only_selected, only_modified;
    gint selected, modified;

    pane = GLDB_STATE_PANE(user_data);

    gtk_tree_model_get(model, iter,
                       COLUMN_STATE_SELECTED_TOTAL, &selected,
                       COLUMN_STATE_MODIFIED_TOTAL, &modified,
                       -1);
    only_selected = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pane->only_selected));
    only_modified = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pane->only_modified));
    if (only_selected && !selected) return FALSE;
    if (only_modified && !modified) return FALSE;
    return TRUE;
}

/* Tracks whether each row is expanded or collapsed within the model.
 * This is necessary because when removing a filter, the rows that are
 * added back to the tree have lost their expansion state.
 */
static void state_row_expanded_collapsed(GtkTreeView *view, GtkTreeIter *iter,
                                         GtkTreePath *path, gpointer user_data)
{
    GtkTreeModel *sort, *filter, *store;
    GtkTreeIter sort_iter, filter_iter, store_iter;
    gboolean valid;

    sort = gtk_tree_view_get_model(view);
    filter = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(sort));
    store = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(filter));
    if (!iter)
    {
        valid = gtk_tree_model_get_iter(filter, &sort_iter, path);
        g_return_if_fail(valid);
    }
    else
        sort_iter = *iter;

    gtk_tree_model_sort_convert_iter_to_child_iter(GTK_TREE_MODEL_SORT(sort),
                                                   &filter_iter, &sort_iter);
    gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(filter),
                                                     &store_iter, &filter_iter);

    gtk_tree_store_set(GTK_TREE_STORE(store), &store_iter,
                       COLUMN_STATE_EXPANDED, (gboolean) GPOINTER_TO_INT(user_data),
                       -1);
}

static void state_save(GtkToolButton *toolbutton,
                       gpointer user_data)
{
    const gldb_state *root;
    GtkWidget *dialog;
    GtkWindow *parent;
    GtkFileFilter *xml_filter, *all_filter;

    parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(toolbutton)));
    if (gldb_get_status() != GLDB_STATUS_STOPPED)
    {
        dialog = gtk_message_dialog_new(parent,
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        "Program is not stopped");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    root = gldb_state_update();
    if (!root)
    {
        dialog = gtk_message_dialog_new(parent,
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        "Could not update state");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    xml_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(xml_filter, "XML files");
    gtk_file_filter_add_mime_type(xml_filter, "text/xml");
    all_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(all_filter, "All files");
    gtk_file_filter_add_pattern(all_filter, "*");

    dialog = gtk_file_chooser_dialog_new("Save State",
                                         parent,
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "state.xml");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), xml_filter);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all_filter);
    if (gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar *filename;
        gchar *state;
        FILE *f;
        gboolean error = false;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        f = fopen(filename, "w");
        gtk_widget_destroy(dialog);
        if (f)
        {
            state = dump_state_xml(root);
            if (fwrite(state, 1, strlen(state), f) != strlen(state))
                error = true;
            g_free(state);
        }
        else
            error = true;
        if (error)
        {
            dialog = gtk_message_dialog_new(parent,
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            "Error writing to %s", filename);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
        g_free(filename);
    }
    else
        gtk_widget_destroy (dialog);
}

static GtkWidget *gldb_state_pane_toolbar_new(GldbStatePane *pane)
{
    GtkWidget *toolbar;
    GtkToolItem *item;

    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

    item = gtk_tool_button_new_from_stock(GTK_STOCK_SAVE);
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(state_save), pane);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

    return toolbar;
}

GldbPane *gldb_state_pane_new(void)
{
    GtkWidget *toolbar, *scrolled, *tree_view, *vbox, *check;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    GldbStatePane *pane;

    pane = GLDB_STATE_PANE(g_object_new(GLDB_STATE_PANE_TYPE, NULL));
    vbox = gtk_vbox_new(FALSE, 0);

    toolbar = gldb_state_pane_toolbar_new(pane);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    pane->state_store = gtk_tree_store_new(8,
                                           G_TYPE_STRING,   /* name */
                                           G_TYPE_STRING,   /* value */
                                           G_TYPE_BOOLEAN,  /* selected */
                                           G_TYPE_BOOLEAN,  /* modified */
                                           G_TYPE_INT,      /* selected-total */
                                           G_TYPE_INT,      /* modified-total */
                                           G_TYPE_INT,      /* boldness */
                                           G_TYPE_BOOLEAN); /* expanded */
    pane->state_filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(pane->state_store), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(pane->state_filter),
                                           state_visible, pane, NULL);
    pane->state_sort = gtk_tree_model_sort_new_with_model(pane->state_filter);
    pane->tree_view = tree_view = gtk_tree_view_new_with_model(pane->state_sort);

    cell = gtk_cell_renderer_toggle_new();
    g_object_set(cell, "yalign", 0.0, NULL);
    g_signal_connect(G_OBJECT(cell), "toggled",
                     G_CALLBACK(state_select_toggled), pane);
    column = gtk_tree_view_column_new_with_attributes(_("Selected"),
                                                      cell,
                                                      "active", COLUMN_STATE_SELECTED,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_STATE_SELECTED);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    g_signal_connect(G_OBJECT(tree_view), "row-expanded",
                     G_CALLBACK(state_row_expanded_collapsed), GINT_TO_POINTER(1));
    g_signal_connect(G_OBJECT(tree_view), "row-collapsed",
                     G_CALLBACK(state_row_expanded_collapsed), GINT_TO_POINTER(0));

    cell = gtk_cell_renderer_text_new();
    g_object_set(cell, "yalign", 0.0, NULL);
    column = gtk_tree_view_column_new_with_attributes(_("Name"),
                                                      cell,
                                                      "text", COLUMN_STATE_NAME,
                                                      "weight", COLUMN_STATE_BOLD,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_STATE_NAME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Value"),
                                                      cell,
                                                      "text", COLUMN_STATE_VALUE,
                                                      "weight", COLUMN_STATE_BOLD,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree_view), COLUMN_STATE_NAME);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), tree_view);
    g_object_unref(G_OBJECT(pane->state_sort)); /* So that it dies with the view */
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    pane->only_selected = check = gtk_check_button_new_with_label(_("Show only selected"));
    g_signal_connect(G_OBJECT(check), "toggled",
                     G_CALLBACK(state_filter_toggled), pane);
    gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);

    pane->only_modified = check = gtk_check_button_new_with_label(_("Show only modified"));
    g_signal_connect(G_OBJECT(check), "toggled",
                     G_CALLBACK(state_filter_toggled), pane);
    gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);

    gldb_pane_set_widget(GLDB_PANE(pane), vbox);
    return GLDB_PANE(pane);
}

static void gldb_state_pane_real_update(GldbPane *self)
{
    GldbStatePane *pane;
    const gldb_state *root;

    if (gldb_get_status() != GLDB_STATUS_DEAD)
    {
        pane = GLDB_STATE_PANE(self);
        root = gldb_state_update();
        if (root)
            update_state_r(root, pane->state_store, NULL);
        else
        {
            GtkWidget *dialog;
            dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(self->top_widget)),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            "Could not update state");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
    }
}

/* GObject stuff */

static void gldb_state_pane_class_init(GldbStatePaneClass *klass)
{
    GldbPaneClass *pane_class;

    pane_class = GLDB_PANE_CLASS(klass);
    pane_class->do_real_update = gldb_state_pane_real_update;
}

static void gldb_state_pane_init(GldbStatePane *self, gpointer g_class)
{
    self->only_selected = NULL;
    self->only_modified = NULL;
    self->tree_view = NULL;
    self->state_store = NULL;
    self->state_filter = NULL;
    self->state_sort = NULL;
}

GType gldb_state_pane_get_type(void)
{
    static GType type = 0;
    if (type == 0)
    {
        static const GTypeInfo info =
        {
            sizeof(GldbStatePaneClass),
            NULL,                       /* base_init */
            NULL,                       /* base_finalize */
            (GClassInitFunc) gldb_state_pane_class_init,
            NULL,                       /* class_finalize */
            NULL,                       /* class_data */
            sizeof(GldbStatePane),
            0,                          /* n_preallocs */
            (GInstanceInitFunc) gldb_state_pane_init,
            NULL                        /* value table */
        };
        type = g_type_register_static(GLDB_PANE_TYPE,
                                      "GldbStatePaneType",
                                      &info, 0);
    }
    return type;
}
