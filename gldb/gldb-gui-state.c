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

/* Functions in gldb-gui that deal with the state viewer. Other
 * (non-GUI) state-handling is done in gldb-common.c
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
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-gui-state.h"

enum
{
    COLUMN_STATE_NAME,
    COLUMN_STATE_VALUE,
    COLUMN_STATE_BOLD,      /* Used to highlight updated state */
    COLUMN_STATE_SELECTED
};

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
            gchar *value;
            gchar *value_utf8, *old_utf8;
            int cmp;

            value = child->value ? child->value : "";
            value_utf8 = g_convert(value, -1, "UTF8", "ASCII", NULL, NULL, NULL);
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                               COLUMN_STATE_VALUE, &old_utf8,
                               -1);
            cmp = strcmp(old_utf8, value_utf8);
            gtk_tree_store_set(store, &iter,
                               COLUMN_STATE_VALUE, value_utf8 ? value_utf8 : "",
                               COLUMN_STATE_BOLD, cmp == 0 ? PANGO_WEIGHT_NORMAL : PANGO_WEIGHT_BOLD,
                               -1);
            g_free(old_utf8);
            g_free(value_utf8);
            update_state_r(child, store, &iter);
            /* Mark as seen for the next phase */
            bugle_hash_set(&lookup, child->name, NULL);
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        }
        else
            valid = gtk_tree_store_remove(store, &iter);
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
            gchar *value;
            gchar *value_utf8;

            value = child->value ? child->value : "";
            value_utf8 = g_convert(value, -1, "UTF8", "ASCII", NULL, NULL, NULL);
            gtk_tree_store_insert_before(store, &iter2, parent,
                                         valid ? &iter : NULL);
            gtk_tree_store_set(store, &iter2,
                               COLUMN_STATE_NAME, child->name,
                               COLUMN_STATE_VALUE, value_utf8 ? value_utf8 : "",
                               COLUMN_STATE_BOLD, PANGO_WEIGHT_BOLD,
                               COLUMN_STATE_SELECTED, FALSE,
                               -1);
            g_free(value_utf8);
            update_state_r(child, store, &iter2);
        }
        else if (valid)
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }

    bugle_hash_clear(&lookup);
}

void state_update(GldbWindowState *state)
{
    if (!state->dirty) return;
    state->dirty = FALSE;
    update_state_r(gldb_state_update(), state->state_store, NULL);
}

void state_mark_dirty(GldbWindowState *state)
{
    state->dirty = TRUE;
}

static void state_select_toggled(GtkCellRendererToggle *cell,
                                 gchar *path,
                                 gpointer user_data)
{
    GldbWindowState *state;
    GtkTreeStore *store;
    GtkTreeModelFilter *filter;
    GtkTreeIter filter_iter, store_iter;
    gboolean selected;

    state = (GldbWindowState *) user_data;
    filter = GTK_TREE_MODEL_FILTER(state->state_filter);
    store = GTK_TREE_STORE(state->state_store);
    if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(filter), &filter_iter, path))
    {
        gtk_tree_model_filter_convert_iter_to_child_iter(filter, &store_iter, &filter_iter);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &store_iter,
                           COLUMN_STATE_SELECTED, &selected, -1);
        gtk_tree_store_set(store, &store_iter,
                           COLUMN_STATE_SELECTED, !selected, -1);
    }
}

static void state_filter_toggled(GtkWidget *widget,
                                 gpointer user_data)
{
    GldbWindowState *state;
    state = (GldbWindowState *) user_data;
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(state->state_filter));
}

/* FIXME: this is horribly slow. However, my attempts to keep a visible field
 * have been dismal failures resulting in mysterious segfaults. A better
 * approach may be to keep counts of selected and modified descendants.
 * FIXME: when the state of X is changed, the ancestors of X need to be
 * refiltered.
 * FIXME: the expansion state of filtered-out rows is lost
 */
static gboolean state_visible(GtkTreeModel *model, GtkTreeIter *iter,
                              gpointer user_data)
{
    GldbWindowState *state;
    gboolean only_selected;
    gboolean only_modified;
    gboolean selected;
    gint bold;
    GtkTreeIter child;
    gboolean valid;

    state = (GldbWindowState *) user_data;

    valid = gtk_tree_model_iter_children(model, &child, iter);
    while (valid)
    {
        if (state_visible(model, &child, user_data)) return TRUE;
        valid = gtk_tree_model_iter_next(model, &child);
    }

    gtk_tree_model_get(model, iter,
                       COLUMN_STATE_SELECTED, &selected,
                       COLUMN_STATE_BOLD, &bold,
                       -1);
    only_selected = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->only_selected));
    only_modified = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->only_modified));
    if (only_selected && !selected) return FALSE;
    if (only_modified && bold != PANGO_WEIGHT_BOLD) return FALSE;
    return TRUE;
}

void state_page_new(GldbWindowState *state, GtkNotebook *notebook)
{
    GtkWidget *scrolled, *tree_view, *label, *vbox, *check;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    gint page;

    vbox = gtk_vbox_new(FALSE, 0);

    state->state_store = gtk_tree_store_new(4,
                                            G_TYPE_STRING,   /* name */
                                            G_TYPE_STRING,   /* value */
                                            G_TYPE_INT,      /* boldness */
                                            G_TYPE_BOOLEAN); /* selected */
    state->state_filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(state->state_store), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(state->state_filter),
                                           state_visible, state, NULL);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(state->state_filter));

    cell = gtk_cell_renderer_toggle_new();
    g_object_set(cell, "yalign", 0.0, NULL);
    g_signal_connect(G_OBJECT(cell), "toggled",
                     G_CALLBACK(state_select_toggled), state);
    column = gtk_tree_view_column_new_with_attributes(_("Selected"),
                                                      cell,
                                                      "active", COLUMN_STATE_SELECTED,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_STATE_SELECTED);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

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
    g_object_unref(G_OBJECT(state->state_filter)); /* So that it dies with the view */
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    state->only_selected = check = gtk_check_button_new_with_label(_("Show only selected"));
    g_signal_connect(G_OBJECT(check), "toggled",
                     G_CALLBACK(state_filter_toggled), state);
    gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);

    state->only_modified = check = gtk_check_button_new_with_label(_("Show only modified"));
    g_signal_connect(G_OBJECT(check), "toggled",
                     G_CALLBACK(state_filter_toggled), state);
    gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);

    label = gtk_label_new(_("State"));
    page = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
    state->dirty = FALSE;
    state->page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page);
}
