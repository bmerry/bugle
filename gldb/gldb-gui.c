/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <gtk/gtk.h>
#include <gtk/gtkgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
    COLUMN_NAME,
    COLUMN_VALUE
} StateTreeColumn;

typedef struct
{
    GtkWidget *window;
    GtkWidget *notebook;
    GtkTreeStore *state_store;
    GtkListStore *backtrace_store;
} GldbWindow;

static void build_state_page(GldbWindow *context)
{
    GtkWidget *tree_view, *label;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    context->state_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(context->state_store));
    cell = gtk_cell_renderer_text_new();
    g_object_set(cell, "yalign", 0.0, NULL);
    column = gtk_tree_view_column_new_with_attributes("Name",
                                                      cell,
                                                      "text", COLUMN_NAME,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Value",
                                                      cell,
                                                      "text", COLUMN_VALUE,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree_view), COLUMN_NAME);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_widget_show(tree_view);

    label = gtk_label_new("State");
    gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook), tree_view, label);
}

static void build_texture_page(GldbWindow *context)
{
    GtkWidget *label, *vbox;

    label = gtk_label_new("Textures");
    vbox = gtk_drawing_area_new();
    gtk_widget_show(vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook), vbox, label);
}

static void build_shader_page(GldbWindow *context)
{
    GtkWidget *label, *vbox;

    label = gtk_label_new("Shaders");
    vbox = gtk_drawing_area_new();
    gtk_widget_show(vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook), vbox, label);
}

static void build_backtrace_page(GldbWindow *context)
{
    GtkWidget *label, *vbox;

    label = gtk_label_new("Backtrace");
    vbox = gtk_drawing_area_new();
    gtk_widget_show(vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook), vbox, label);
}

static void build_main_window(GldbWindow *context)
{
    context->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(context->window), "gldb-gui");
    g_signal_connect(G_OBJECT(context->window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    context->notebook = gtk_notebook_new();

    build_state_page(context);
    build_texture_page(context);
    build_shader_page(context);
    build_backtrace_page(context);

    gtk_widget_show(context->notebook);
    gtk_container_add(GTK_CONTAINER(context->window), context->notebook);
    gtk_window_set_default_size(GTK_WINDOW(context->window), 640, 480);
    gtk_widget_show(context->window);
}

static void update_state(GldbWindow *context)
{
    GtkTreeIter iter, child;

    gtk_tree_store_append(context->state_store, &iter, NULL);
    gtk_tree_store_set(context->state_store, &iter,
                       COLUMN_NAME, "Some state",
                       COLUMN_VALUE, "Some value",
                       -1);

    gtk_tree_store_append(context->state_store, &child, &iter);
    gtk_tree_store_set(context->state_store, &child,
                       COLUMN_NAME, "Some child state",
                       COLUMN_VALUE, "Some\nmultiline\nvalue",
                       -1);

    gtk_tree_store_append(context->state_store, &child, &iter);
    gtk_tree_store_set(context->state_store, &child,
                       COLUMN_NAME, "Some other child state",
                       COLUMN_VALUE, "Some other child value",
                       -1);
}

int main(int argc, char **argv)
{
    GldbWindow context;

    gtk_init(&argc, &argv);
    gtk_gl_init(&argc, &argv);

    build_main_window(&context);
    update_state(&context);

    gtk_main();
    return 0;
}
