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
#include "common/hashtable.h"
#include "common/safemem.h"
#include "src/names.h"
#include "gldb/gldb-common.h"

enum
{
    STATE_COLUMN_NAME,
    STATE_COLUMN_VALUE
};

enum
{
    BREAKPOINTS_COLUMN_FUNCTION
};

typedef struct
{
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *statusbar;
    guint statusbar_context_id;
    GtkActionGroup *actions;
    GtkActionGroup *run_actions;
    GtkActionGroup *norun_actions;

    GtkTreeStore *state_store;
    GtkListStore *backtrace_store;
    GtkListStore *breakpoints_store;

    GIOChannel *channel;
    guint channel_watch;
} GldbWindow;

typedef struct
{
    GldbWindow *parent;
    GtkWidget *dialog, *list;
} GldbBreakpointsDialog;

static GtkListStore *function_names;

/* We can't just rip out all the old state and plug in the new, because
 * that loses any expansions that may have been active. Instead, we
 * take each state in the store and try to match it with state in the
 * gldb state tree. If it fails, we delete the state. Any new state also
 * has to be placed in the correct position.
 */
static void update_state_r(const gldb_state *root, GtkTreeStore *store,
                           GtkTreeIter *parent)
{
    /* FIXME: redesign bugle_hash_get to always return the value, even
     * if it is NULL, then eliminate seen.
     */
    bugle_hash_table lookup, seen;
    GtkTreeIter iter, iter2;
    gboolean valid;
    bugle_list_node *i;
    const gldb_state *child;
    gchar *name;

    bugle_hash_init(&lookup, false);
    bugle_hash_init(&seen, false);

    /* Build lookup table */
    for (i = bugle_list_head(&root->children); i; i = bugle_list_next(i))
    {
        child = (const gldb_state *) bugle_list_data(i);
        if (child->name) bugle_hash_set(&lookup, child->name, (void *) child);
    }

    /* Update common, and remove items from store not present in state */
    valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &iter, parent);
    while (valid)
    {
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, STATE_COLUMN_NAME, &name, -1);
        child = (const gldb_state *) bugle_hash_get(&lookup, name);
        g_free(name);
        if (child)
        {
            gtk_tree_store_set(store, &iter, STATE_COLUMN_VALUE, child->value, -1);
            update_state_r(child, store, &iter);
            bugle_hash_set(&seen, child->name, NULL);
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
        if (!bugle_hash_get(&seen, child->name))
        {
            gtk_tree_store_insert_before(store, &iter2, parent,
                                         valid ? &iter : NULL);
            gtk_tree_store_set(store, &iter2,
                               STATE_COLUMN_NAME, child->name,
                               STATE_COLUMN_VALUE, child->value ? child->value : "",
                               -1);
            update_state_r(child, store, &iter2);
        }
        else if (valid)
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }

    bugle_hash_clear(&lookup);
    bugle_hash_clear(&seen);
}

static void update_state(GldbWindow *context)
{
    update_state_r(gldb_state_update(), context->state_store, NULL);
}

static void update_backtrace(GldbWindow *context)
{
    gchar *argv[4];
    gint in, out;
    GError *error = NULL;
    GIOChannel *inf, *outf;
    gchar *line;
    gsize terminator;
    const gchar *cmds = "set width 0\nset height 0\nbacktrace\nquit\n";

    argv[0] = g_strdup("gdb");
    argv[1] = g_strdup(gldb_program());
    bugle_asprintf(&argv[2], "%lu", (unsigned long) gldb_child_pid());
    argv[3] = NULL;
    if (g_spawn_async_with_pipes(NULL,    /* working directory */
                                 argv,
                                 NULL,    /* envp */
                                 G_SPAWN_SEARCH_PATH,
                                 NULL,    /* setup func */
                                 NULL,    /* user_data */
                                 NULL,    /* PID pointer */
                                 &in,
                                 &out,
                                 NULL,    /* stderr pointer */
                                 &error))
    {
        /* Note: in and out refer to the child's view, so we swap here */
        inf = g_io_channel_unix_new(out);
        outf = g_io_channel_unix_new(in);
        g_io_channel_set_close_on_unref(outf, TRUE);
        g_io_channel_set_close_on_unref(inf, TRUE);
        g_io_channel_set_encoding(inf, NULL, NULL);
        g_io_channel_set_encoding(outf, NULL, NULL);
        g_io_channel_write_chars(outf, cmds, strlen(cmds), NULL, NULL);
        g_io_channel_flush(outf, NULL);

        gtk_list_store_clear(context->backtrace_store);
        while (g_io_channel_read_line(inf, &line, NULL, &terminator, &error) == G_IO_STATUS_NORMAL
               && line != NULL)
            if (line[0] == '#')
            {
                GtkTreeIter iter;

                line[terminator] = '\0';
                gtk_list_store_append(context->backtrace_store, &iter);
                gtk_list_store_set(context->backtrace_store, &iter,
                                   0, line, -1);
                g_free(line);
            }
        g_io_channel_unref(inf);
        g_io_channel_unref(outf);
    }
    g_free(argv[0]);
    g_free(argv[1]);
    free(argv[2]);
}

static void update_status_bar(GldbWindow *context, const gchar *text)
{
    gtk_statusbar_pop(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id);
    gtk_statusbar_push(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id, text);
}

static void stopped(GldbWindow *context)
{
    gldb_set_status(GLDB_STATUS_STOPPED);
    update_state(context);
    update_backtrace(context);
    update_status_bar(context, "Stopped");
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

    gldb_set_status(GLDB_STATUS_DEAD);
    update_status_bar(context, "Not running");
    gtk_action_group_set_sensitive(context->run_actions, FALSE);
    gtk_action_group_set_sensitive(context->norun_actions, TRUE);
    gtk_list_store_clear(context->backtrace_store);
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
        gldb_set_status(GLDB_STATUS_RUNNING);
        update_status_bar(context, "Running");
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
    if (gldb_get_status() != GLDB_STATUS_DEAD)
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
        update_status_bar(context, "Started");
    }
}

static void stop_action(GtkAction *action, gpointer user_data)
{
    gldb_send_async();
}

static void continue_action(GtkAction *action, gpointer user_data)
{
    gldb_send_continue();
}

static void kill_action(GtkAction *action, gpointer user_data)
{
    gldb_send_quit();
}

static void chain_action(GtkAction *action, gpointer user_data)
{
    GtkWidget *dialog, *entry;
    GldbWindow *context;
    gint result;

    context = (GldbWindow *) user_data;
    dialog = gtk_dialog_new_with_buttons("Filter chain",
                                         GTK_WINDOW(context->window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    entry = gtk_entry_new();
    if (gldb_get_chain())
        gtk_entry_set_text(GTK_ENTRY(entry), gldb_get_chain());
    gtk_widget_show(entry);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), entry, TRUE, FALSE, 0);

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT)
        gldb_set_chain(gtk_entry_get_text(GTK_ENTRY(entry)));
    gtk_widget_destroy(dialog);
}

static void breakpoints_add_action(GtkButton *button, gpointer user_data)
{
    GldbBreakpointsDialog *context;
    GtkWidget *dialog, *entry;
    GtkEntryCompletion *completion;
    gint result;

    context = (GldbBreakpointsDialog *) user_data;

    dialog = gtk_dialog_new_with_buttons("Add breakpoint",
                                         GTK_WINDOW(context->dialog),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    entry = gtk_entry_new();
    completion = gtk_entry_completion_new();
    gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(function_names));
    gtk_entry_completion_set_minimum_key_length(completion, 4);
    gtk_entry_completion_set_text_column(completion, 0);
    gtk_entry_set_completion(GTK_ENTRY(entry), completion);
    gtk_widget_show(entry);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), entry, TRUE, FALSE, 0);

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT)
    {
        GtkTreeIter iter;

        gtk_list_store_append(context->parent->breakpoints_store, &iter);
        gtk_list_store_set(context->parent->breakpoints_store, &iter,
                           BREAKPOINTS_COLUMN_FUNCTION, gtk_entry_get_text(GTK_ENTRY(entry)),
                           -1);
        gldb_set_break(gtk_entry_get_text(GTK_ENTRY(entry)), true);
    }
    gtk_widget_destroy(dialog);
}

static void breakpoints_remove_action(GtkButton *button, gpointer user_data)
{
    GldbBreakpointsDialog *context;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *model;
    const gchar *func;

    context = (GldbBreakpointsDialog *) user_data;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(context->list));
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_tree_model_get(model, &iter, 0, &func, -1);
        gldb_set_break(func, false);
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    }
}

static void breakpoints_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *pcontext;
    GldbBreakpointsDialog context;
    GtkWidget *hbox, *bbox, *btn, *toggle;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    gint result;

    pcontext = (GldbWindow *) user_data;
    context.parent = pcontext;
    context.dialog = gtk_dialog_new_with_buttons("Breakpoints",
                                                 GTK_WINDOW(pcontext->window),
                                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                                 NULL);

    hbox = gtk_hbox_new(FALSE, 0);

    context.list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pcontext->breakpoints_store));
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Function",
                                                      cell,
                                                      "text", BREAKPOINTS_COLUMN_FUNCTION,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(context.list), column);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(context.list), FALSE);
    gtk_widget_set_size_request(context.list, 300, 200);
    gtk_box_pack_start(GTK_BOX(hbox), context.list, TRUE, TRUE, 0);
    gtk_widget_show(context.list);

    bbox = gtk_vbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_SPREAD);
    btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(breakpoints_add_action), &context);
    gtk_widget_show(btn);
    gtk_box_pack_start(GTK_BOX(bbox), btn, TRUE, FALSE, 0);
    btn = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(breakpoints_remove_action), &context);
    gtk_widget_show(btn);
    gtk_box_pack_start(GTK_BOX(bbox), btn, TRUE, FALSE, 0);
    gtk_widget_show(bbox);
    gtk_box_pack_start(GTK_BOX(hbox), bbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox), hbox, TRUE, TRUE, 0);

    toggle = gtk_check_button_new_with_mnemonic("Break on _errors");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), gldb_get_break_error());
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(context.dialog)->vbox), toggle, FALSE, FALSE, 0);
    gtk_widget_show(toggle);

    gtk_dialog_set_default_response(GTK_DIALOG(context.dialog), GTK_RESPONSE_ACCEPT);

    result = gtk_dialog_run(GTK_DIALOG(context.dialog));
    if (result == GTK_RESPONSE_ACCEPT)
        gldb_set_break_error(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));
    gtk_widget_destroy(context.dialog);
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
                                                      "text", STATE_COLUMN_NAME,
                                                      NULL);
    gtk_tree_view_column_set_sort_column_id(column, STATE_COLUMN_NAME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Value",
                                                      cell,
                                                      "text", STATE_COLUMN_VALUE,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree_view), STATE_COLUMN_NAME);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_view), TRUE);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), tree_view);
    gtk_widget_show(scrolled);
    g_object_unref(G_OBJECT(context->state_store)); /* So that it dies with the view */

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
    GtkWidget *label, *view, *scrolled;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;

    context->backtrace_store = gtk_list_store_new(1, G_TYPE_STRING);
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Call",
                                                      cell,
                                                      "text", 0,
                                                      NULL);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(context->backtrace_store));
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), false);
    gtk_widget_show(view);
    g_object_unref(G_OBJECT(context->backtrace_store)); /* So that it dies with the view */

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), view);

    label = gtk_label_new("Backtrace");
    gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook), scrolled, label);
}

static const gchar *ui_desc =
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Quit' />"
"    </menu>"
"    <menu action='OptionsMenu'>"
"      <menuitem action='Chain' />"
"    </menu>"
"    <menu action='RunMenu'>"
"      <menuitem action='Run' />"
"      <menuitem action='Stop' />"
"      <menuitem action='Continue' />"
"      <menuitem action='Kill' />"
"      <separator />"
"      <menuitem action='Breakpoints' />"
"    </menu>"
"  </menubar>"
"</ui>";

static GtkActionEntry action_desc[] =
{
    { "FileMenu", NULL, "_File", NULL, NULL, NULL },
    { "Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q", NULL, G_CALLBACK(quit_action) },

    { "OptionsMenu", NULL, "_Options", NULL, NULL, NULL },
    { "Chain", NULL, "Filter _Chain", NULL, NULL, G_CALLBACK(chain_action) },

    { "RunMenu", NULL, "_Run", NULL, NULL, NULL },
    { "Breakpoints", NULL, "_Breakpoints...", NULL, NULL, G_CALLBACK(breakpoints_action) }
};

static GtkActionEntry run_action_desc[] =
{
    { "Stop", NULL, "_Stop", "<control>Break", NULL, G_CALLBACK(stop_action) },
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

    context->breakpoints_store = gtk_list_store_new(1, G_TYPE_STRING);
    context->notebook = gtk_notebook_new();
    build_state_page(context);
    build_texture_page(context);
    build_shader_page(context);
    build_backtrace_page(context);
    gtk_box_pack_start(GTK_BOX(vbox), context->notebook, TRUE, TRUE, 0);

    context->statusbar = gtk_statusbar_new();
    context->statusbar_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(context->statusbar), "Program status");
    gtk_statusbar_push(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id, "Not running");
    gtk_box_pack_start(GTK_BOX(vbox), context->statusbar, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(context->window), vbox);
    gtk_window_set_default_size(GTK_WINDOW(context->window), 640, 480);
    gtk_window_add_accel_group (GTK_WINDOW(context->window),
                                gtk_ui_manager_get_accel_group(ui));
    gtk_widget_show_all(context->window);
}

static void build_function_names(void)
{
    gsize i;
    GtkTreeIter iter;

    function_names = gtk_list_store_new(1, G_TYPE_STRING);
    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
    {
        gtk_list_store_append(function_names, &iter);
        gtk_list_store_set(function_names, &iter, 0, budgie_function_names[i], -1);
    }
}

int main(int argc, char **argv)
{
    GldbWindow context;

    gtk_init(&argc, &argv);
    gldb_initialise(argc, argv);
    bugle_initialise_hashing();

    build_function_names();
    memset(&context, 0, sizeof(context));
    build_main_window(&context);

    gtk_main();

    gldb_shutdown();
    g_object_unref(G_OBJECT(function_names));
    g_object_unref(G_OBJECT(context.breakpoints_store));
    return 0;
}
