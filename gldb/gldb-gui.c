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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/protocol.h"
#include "common/linkedlist.h"
#include "gldb/gldb-common.h"

typedef enum
{
    COLUMN_NAME,
    COLUMN_VALUE
} StateTreeColumn;

typedef struct
{
    GtkWidget *window;
    GtkWidget *notebook;
    GtkActionGroup *actions;
    GtkActionGroup *run_actions;
    GtkActionGroup *norun_actions;

    GtkTreeStore *state_store;
    GtkListStore *backtrace_store;

    GIOChannel *channel;
    guint channel_watch;
} GldbWindow;

static void update_state_r(const gldb_state *root, GtkTreeStore *store,
                           GtkTreeIter *parent)
{
    GtkTreeIter iter;
    bugle_list_node *i;
    const gldb_state *child;

    for (i = bugle_list_head(&root->children); i; i = bugle_list_next(i))
    {
        child = (const gldb_state *) bugle_list_data(i);

        gtk_tree_store_append(store, &iter, parent);
        gtk_tree_store_set(store, &iter,
                           COLUMN_NAME, child->name ? child->name : "",
                           COLUMN_VALUE, child->value ? child->value : "",
                           -1);
        update_state_r(child, store, &iter);
    }
}

static void update_state(GldbWindow *context)
{
    gtk_tree_store_clear(context->state_store);
    update_state_r(gldb_state_update(), context->state_store, NULL);
}

static void stopped(GldbWindow *context)
{
    gldb_info_stopped();
    update_state(context);
}

static void child_exit_callback(GPid pid, gint status, gpointer data)
{
    GldbWindow *context;
    GError *error = NULL;

    context = (GldbWindow *) data;
    if (context->channel)
    {
        g_io_channel_shutdown(context->channel, TRUE, &error);
        g_io_channel_unref(context->channel);
        context->channel = NULL;
        g_source_remove(context->channel_watch);
        context->channel_watch = 0;
    }

    g_message("Child exited");
    gldb_info_child_terminated();
    gtk_action_group_set_sensitive(context->run_actions, FALSE);
    gtk_action_group_set_sensitive(context->norun_actions, TRUE);
}

static gboolean response_callback(GIOChannel *channel, GIOCondition condition,
                                  gpointer data)
{
    uint32_t resp;
    uint32_t resp_val, resp_len;
    char *resp_str, *resp_str2;
    FILE *f;
    int lib_in;
    GldbWindow *context;

    context = (GldbWindow *) data;
    lib_in = gldb_in_pipe();
    if (!gldb_protocol_recv_code(lib_in, &resp))
    {
        fputs("Pipe error", stderr);
        exit(1);
    }
    switch (resp)
    {
    case RESP_ANS:
        gldb_protocol_recv_code(lib_in, &resp_val);
        /* Ignore, other than to flush the pipe */
        break;
    case RESP_STOP:
        stopped(context);
        break;
    case RESP_BREAK:
        gldb_protocol_recv_string(lib_in, &resp_str);
        g_message("Break on %s", resp_str);
        free(resp_str);
        stopped(context);
        break;
    case RESP_BREAK_ERROR:
        gldb_protocol_recv_string(lib_in, &resp_str);
        gldb_protocol_recv_string(lib_in, &resp_str2);
        g_message("Error %s in %s", resp_str2, resp_str);
        free(resp_str);
        free(resp_str2);
        stopped(context);
        break;
    case RESP_ERROR:
        gldb_protocol_recv_code(lib_in, &resp_val);
        gldb_protocol_recv_string(lib_in, &resp_str);
        g_message("%s\n", resp_str);
        free(resp_str);
        break;
    case RESP_RUNNING:
        g_message("Running.\n");
        gldb_info_running();
        gtk_action_group_set_sensitive(context->run_actions, TRUE);
        gtk_action_group_set_sensitive(context->norun_actions, FALSE);
        break;
    case RESP_STATE:
        gldb_protocol_recv_string(lib_in, &resp_str);
        fputs(resp_str, stdout);
        free(resp_str);
        break;
    case RESP_SCREENSHOT:
        gldb_protocol_recv_binary_string(lib_in, &resp_len, &resp_str);
#if 0 /* FIXME */
        if (!screenshot_file)
        {
            g_warning("Unexpected screenshot data. Please contact the author.\n");
            break;
        }
        f = fopen(screenshot_file, "wb");
        if (!f)
        {
            g_warning("Cannot open %s: %s\n", screenshot_file, strerror(errno));
            free(screenshot_file);
            break;
        }
        if (fwrite(resp_str, 1, resp_len, f) != resp_len || fclose(f) == EOF)
            g_warning("Error writing %s: %s\n", screenshot_file, strerror(errno));
        free(resp_str);
        free(screenshot_file);
        screenshot_file = NULL;
#endif
        break;
    }

    return TRUE;
}

static void quit_action(GtkAction *action, gpointer user_data)
{
    gtk_widget_destroy(((GldbWindow *) user_data)->window);
}

static void child_init(void)
{
}

static void run_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    if (gldb_running())
    {
        /* FIXME */
    }
    else
    {
        /* GIOChannel on UNIX is sufficiently light that we may wrap
         * the input pipe in it without forcing us to do everything through
         * it.
         */
        gldb_run(child_init);
        context->channel = g_io_channel_unix_new(gldb_in_pipe());
        context->channel_watch = g_io_add_watch(context->channel, G_IO_IN | G_IO_ERR,
                                                response_callback, context);
        g_child_watch_add(gldb_child_pid(), child_exit_callback, context);
    }
}

static void break_action(GtkAction *action, gpointer user_data)
{
    if (gldb_running())
        gldb_send_async();
}

static void continue_action(GtkAction *action, gpointer user_data)
{
    if (gldb_running())
        gldb_send_continue();
}

static void kill_action(GtkAction *action, gpointer user_data)
{
    if (gldb_running())
        gldb_send_quit();
}

static void build_state_page(GldbWindow *context)
{
    GtkWidget *scrolled, *tree_view, *label;
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
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_NAME);
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

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), tree_view);
    gtk_widget_show(scrolled);

    label = gtk_label_new("State");
    gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook), scrolled, label);
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

static const gchar *ui_desc =
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Quit' />"
"    </menu>"
"    <menu action='RunMenu'>"
"      <menuitem action='Run' />"
"      <separator />"
"      <menuitem action='Stop' />"
"      <menuitem action='Continue' />"
"      <menuitem action='Kill' />"
"    </menu>"
"  </menubar>"
"</ui>";

static GtkActionEntry action_desc[] =
{
    { "FileMenu", NULL, "_File", NULL, NULL, NULL },
    { "Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q", NULL, G_CALLBACK(quit_action) },

    { "RunMenu", NULL, "_Run", NULL, NULL, NULL },
};

static GtkActionEntry run_action_desc[] =
{
    { "Stop", NULL, "_Stop", "<control>Break", NULL, G_CALLBACK(break_action) },
    { "Continue", NULL, "_Continue", "<control>F9", NULL, G_CALLBACK(continue_action) },
    { "Kill", NULL, "_Kill", "<control>F2", NULL, G_CALLBACK(kill_action) }
};

static GtkActionEntry norun_action_desc[] =
{
    { "Run", NULL, "_Run", NULL, NULL, G_CALLBACK(run_action) }
};

static GtkUIManager *build_ui(GldbWindow *context)
{
    GtkUIManager *ui;
    GError *error = NULL;

    context->actions = gtk_action_group_new("Actions");
    gtk_action_group_add_actions(context->actions, action_desc,
                                 G_N_ELEMENTS(action_desc), context);
    context->run_actions = gtk_action_group_new("RunActions");
    gtk_action_group_add_actions(context->run_actions, run_action_desc,
                                 G_N_ELEMENTS(run_action_desc), context);
    context->norun_actions = gtk_action_group_new("NorunActions");
    gtk_action_group_add_actions(context->norun_actions, norun_action_desc,
                                 G_N_ELEMENTS(norun_action_desc), context);
    gtk_action_group_set_sensitive(context->run_actions, FALSE);

    ui = gtk_ui_manager_new();
    gtk_ui_manager_set_add_tearoffs(ui, TRUE);
    gtk_ui_manager_insert_action_group(ui, context->actions, 0);
    gtk_ui_manager_insert_action_group(ui, context->run_actions, 0);
    gtk_ui_manager_insert_action_group(ui, context->norun_actions, 0);
    if (!gtk_ui_manager_add_ui_from_string(ui, ui_desc, strlen(ui_desc), &error))
    {
        g_message("Failed to create UI: %s", error->message);
        g_error_free(error);
    }
    return ui;
}

static void build_main_window(GldbWindow *context)
{
    GtkUIManager *ui;
    GtkWidget *vbox;

    context->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(context->window), "gldb-gui");
    g_signal_connect(G_OBJECT(context->window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    ui = build_ui(context);
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_ui_manager_get_widget(ui, "/MenuBar"),
                       FALSE, FALSE, 0);

    context->notebook = gtk_notebook_new();
    build_state_page(context);
    build_texture_page(context);
    build_shader_page(context);
    build_backtrace_page(context);
    gtk_box_pack_start(GTK_BOX(vbox), context->notebook, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(context->window), vbox);
    gtk_window_set_default_size(GTK_WINDOW(context->window), 640, 480);
    gtk_window_add_accel_group (GTK_WINDOW(context->window),
                                gtk_ui_manager_get_accel_group(ui));
    gtk_widget_show_all(context->window);
}

int main(int argc, char **argv)
{
    GldbWindow context;

    gtk_init(&argc, &argv);
    gldb_initialise(argc, argv);

    build_main_window(&context);

    gtk_main();
    return 0;
}
