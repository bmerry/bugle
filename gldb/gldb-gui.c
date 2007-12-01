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
#if HAVE_GTKGLEXT
# include <gtk/gtkgl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "common/protocol.h"
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include "src/names.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-channels.h"
#include "gldb/gldb-gui-target.h"
#include "gldb/gldb-gui-image.h"
#include "gldb/gldb-gui-state.h"
#include "gldb/gldb-gui-texture.h"
#include "gldb/gldb-gui-framebuffer.h"
#include "gldb/gldb-gui-shader.h"
#include "gldb/gldb-gui-backtrace.h"
#include "gldb/gldb-gui-breakpoint.h"

/* Global variables */
static linked_list response_handlers;
static guint32 seq = 0;
static GdkCursor *wait_cursor = NULL;

/* gldb-gui is broken into a number of separate modules, which are unified by
 * the GldbPane type. A GldbPane encapsulates a widget plus internal state,
 * which is inserted into the notebook. Future work may allow panes to be
 * split out into separate windows.
 */
static void gldb_pane_real_status_changed(GldbPane *self, gldb_status new_status)
{
}

static void gldb_pane_class_init(GldbPaneClass *klass)
{
    klass->do_real_update = NULL;    /* pure virtual function */
    klass->do_status_changed = gldb_pane_real_status_changed; /* no-op */
}

static void gldb_pane_init(GldbPane *self)
{
    self->dirty = FALSE;
    self->top_widget = NULL;
}

GType gldb_pane_get_type(void)
{
    static GType type = 0;
    if (type == 0)
    {
        static const GTypeInfo info =
        {
            sizeof(GldbPaneClass),
            NULL,                       /* base_init */
            NULL,                       /* base_finalize */
            (GClassInitFunc) gldb_pane_class_init,
            NULL,                       /* class_finalize */
            NULL,                       /* class_data */
            sizeof(GldbPane),
            0,                          /* n_preallocs */
            (GInstanceInitFunc) gldb_pane_init,
            NULL                        /* value table */
        };
        type = g_type_register_static(G_TYPE_OBJECT,
                                      "GldbPaneType",
                                      &info, G_TYPE_FLAG_ABSTRACT);
    }
    return type;
}

void gldb_pane_update(GldbPane *self)
{
    if (self->dirty)
    {
        GdkWindow *window = NULL;
        GtkWidget *widget;

        self->dirty = FALSE;
        /* Show an hourglass/watch while waiting for potentially slow update */
        widget = gtk_widget_get_toplevel(gldb_pane_get_widget(self));
        if (GTK_WIDGET_REALIZED(widget))
        {
            window = widget->window;
            gdk_window_set_cursor(window, wait_cursor);
            gdk_display_flush(gtk_widget_get_display(widget));
        }
        GLDB_PANE_GET_CLASS(self)->do_real_update(self);
        if (window)
            gdk_window_set_cursor(window, NULL);
    }
}

void gldb_pane_invalidate(GldbPane *self)
{
    self->dirty = TRUE;
}

GtkWidget *gldb_pane_get_widget(GldbPane *self)
{
    return self->top_widget;
}

void gldb_pane_set_widget(GldbPane *self, GtkWidget *widget)
{
    self->top_widget = widget;
}

void gldb_pane_status_changed(GldbPane *self, gldb_status new_status)
{
    GLDB_PANE_GET_CLASS(self)->do_status_changed(self, new_status);
}

typedef struct
{
    guint32 id;
    gboolean (*callback)(gldb_response *, gpointer user_data);
    gpointer user_data;
} response_handler;

typedef struct GldbWindow
{
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *statusbar;
    guint statusbar_context_id;

    GtkActionGroup *actions;
    GtkActionGroup *running_actions;
    GtkActionGroup *stopped_actions;
    GtkActionGroup *live_actions;
    GtkActionGroup *dead_actions;

    GIOChannel *channel;
    guint channel_watch;

    GPtrArray *panes;
} GldbWindow;

/* Utility functions for panes to call */

/* Registers a callback for when a particular response is received. The
 * return value is a sequence number that may be passed to functions like
 * gldb_send_data_texture.
 */
guint32 gldb_gui_set_response_handler(gboolean (*callback)(gldb_response *r, gpointer user_data),
                                      gpointer user_data)
{
    response_handler *h;

    h = bugle_malloc(sizeof(response_handler));
    h->id = seq++;
    h->callback = callback;
    h->user_data = user_data;
    bugle_list_append(&response_handlers, h);
    return h->id;
}

/* Saves some of the current state of a combo box into the given array.
 * This array can then be passed to gldb_gui_combo_box_restore_old, which
 * will attempt to find an entry that has the same attributes and will then
 * set that element in the combo box. If there is no current value, the
 * GValue elements are unset. The variadic arguments are the column IDs to
 * use, followed by -1. The array must have enough storage.
 *
 * Currently these function are limited to 4 identifying columns, and
 * to guint, gint and gdouble types.
 */
void gldb_gui_combo_box_get_old(GtkComboBox *box, GValue *save, ...)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    va_list ap;
    int col;
    gboolean have_iter;

    va_start(ap, save);
    model = gtk_combo_box_get_model(box);
    have_iter = gtk_combo_box_get_active_iter(box, &iter);
    while ((col = va_arg(ap, int)) != -1)
    {
        memset(save, 0, sizeof(GValue));
        if (have_iter)
            gtk_tree_model_get_value(model, &iter, col, save);
        save++;
    }
    va_end(ap);
}

/* Note: this function automatically releases the GValues, so it is
 * important to match with the previous one, or else release values
 * manually.
 */
void gldb_gui_combo_box_restore_old(GtkComboBox *box, GValue *save, ...)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GValue cur;
    va_list ap;
    int col, cols[4];
    int ncols = 0;
    int i;
    gboolean more, match;

    va_start(ap, save);
    while ((col = va_arg(ap, int)) != -1)
    {
        g_return_if_fail(ncols < 4);
        cols[ncols++] = col;
    }
    va_end(ap);

    model = gtk_combo_box_get_model(box);
    if (!G_IS_VALUE(&save[0]))
    {
        /* No previous, so set to first item */
        if (gtk_tree_model_get_iter_first(model, &iter))
            gtk_combo_box_set_active_iter(box, &iter);
        return;
    }

    memset(&cur, 0, sizeof(cur));
    more = gtk_tree_model_get_iter_first(model, &iter);
    while (more)
    {
        match = TRUE;
        for (i = 0; i < ncols && match; i++)
        {
            gtk_tree_model_get_value(model, &iter, cols[i], &cur);
            switch (gtk_tree_model_get_column_type(model, cols[i]))
            {
            case G_TYPE_INT:
                match = g_value_get_int(&save[i]) == g_value_get_int(&cur);
                break;
            case G_TYPE_UINT:
                match = g_value_get_uint(&save[i]) == g_value_get_uint(&cur);
                break;
            case G_TYPE_DOUBLE:
                match = g_value_get_double(&save[i]) == g_value_get_double(&cur);
                break;
            default:
                g_return_if_reached();
            }
            g_value_unset(&cur);
        }
        if (match)
        {
            for (i = 0; i < ncols; i++)
                g_value_unset(&save[i]);
            gtk_combo_box_set_active_iter(box, &iter);
            return;
        }

        more = gtk_tree_model_iter_next(model, &iter);
    }
    gtk_combo_box_set_active(box, 0);
}

/******************************************************/

/* Internal utility functions */

static void update_status_bar(GldbWindow *context, const gchar *text)
{
    gtk_statusbar_pop(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id);
    gtk_statusbar_push(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id, text);
}

static void pane_invalidate_helper(gpointer item, gpointer user_data)
{
    gldb_pane_invalidate(GLDB_PANE(item));
}

static void pane_status_changed_helper(gpointer item, gpointer user_data)
{
    gldb_pane_status_changed(GLDB_PANE(item), gldb_get_status());
}

static void pane_status_changed(GldbWindow *context)
{
    g_ptr_array_foreach(context->panes, pane_status_changed_helper, NULL);
}

static void notebook_update(GldbWindow *context, gint new_page)
{
    gint page;
    GtkNotebook *notebook;
    GldbPane *pane;

    notebook = GTK_NOTEBOOK(context->notebook);
    if (new_page >= 0) page = new_page;
    else page = gtk_notebook_get_current_page(notebook);

    pane_status_changed(context);
    pane = GLDB_PANE(g_ptr_array_index(context->panes, page));
    gldb_pane_update(pane);
}

static void stopped(GldbWindow *context, const gchar *text)
{
    g_ptr_array_foreach(context->panes, pane_invalidate_helper, NULL);
    notebook_update(context, -1);
    gtk_action_group_set_sensitive(context->running_actions, FALSE);
    gtk_action_group_set_sensitive(context->stopped_actions, TRUE);
    update_status_bar(context, text);
    pane_status_changed(context);
}

static void main_window_add_pane(GldbWindow *context, gchar *title, GldbPane *pane)
{
    GtkWidget *label;

    label = gtk_label_new(title);
    gtk_notebook_append_page(GTK_NOTEBOOK(context->notebook),
                             gldb_pane_get_widget(pane), label);
    g_ptr_array_add(context->panes, pane);
}

/************************************************************/

/* Signal handlers */

static void notebook_switch_page(GtkNotebook *notebook,
                                 GtkNotebookPage *page,
                                 guint page_num,
                                 gpointer user_data)
{
    notebook_update((GldbWindow *) user_data, page_num);
}

static void child_exit_callback(GPid pid, gint status, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    if (context->channel)
    {
        g_io_channel_unref(context->channel);
        context->channel = NULL;
        g_source_remove(context->channel_watch);
        context->channel_watch = 0;
    }

    gldb_notify_child_dead();
    update_status_bar(context, _("Not running"));
    gtk_action_group_set_sensitive(context->running_actions, FALSE);
    gtk_action_group_set_sensitive(context->stopped_actions, FALSE);
    gtk_action_group_set_sensitive(context->live_actions, FALSE);
    gtk_action_group_set_sensitive(context->dead_actions, TRUE);
    pane_status_changed(context);
}

static gboolean response_callback(GIOChannel *channel, GIOCondition condition,
                                  gpointer user_data)
{
    GldbWindow *context;
    gldb_response *r;
    char *msg;
    gboolean done = FALSE;
    linked_list_node *n;
    response_handler *h;

    context = (GldbWindow *) user_data;
    r = gldb_get_response();

    n = bugle_list_head(&response_handlers);
    if (n) h = (response_handler *) bugle_list_data(n);
    while (n && h->id < r->id)
    {
        bugle_list_erase(&response_handlers, n);
        n = bugle_list_head(&response_handlers);
        if (n) h = (response_handler *) bugle_list_data(n);
    }
    while (n && h->id == r->id)
    {
        done = (*h->callback)(r, h->user_data);
        bugle_list_erase(&response_handlers, n);
        if (done) return TRUE;
        n = bugle_list_head(&response_handlers);
        if (n) h = (response_handler *) bugle_list_data(n);
    }

    switch (r->code)
    {
    case RESP_STOP:
        bugle_asprintf(&msg, _("Stopped in %s"), ((gldb_response_stop *) r)->call);
        stopped(context, msg);
        free(msg);
        break;
    case RESP_BREAK:
        bugle_asprintf(&msg, _("Break on %s"), ((gldb_response_break *) r)->call);
        stopped(context, msg);
        free(msg);
        break;
    case RESP_BREAK_ERROR:
        bugle_asprintf(&msg, _("%s in %s"),
                       ((gldb_response_break_error *) r)->error,
                       ((gldb_response_break_error *) r)->call);
        stopped(context, msg);
        free(msg);
        break;
    case RESP_ERROR:
        {
            GtkWidget *dialog;
            dialog = gtk_message_dialog_new(GTK_WINDOW(context->window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            "%s",
                                            ((gldb_response_error *) r)->error);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
        }
        break;
    case RESP_RUNNING:
        update_status_bar(context, _("Running"));
        gtk_action_group_set_sensitive(context->running_actions, TRUE);
        gtk_action_group_set_sensitive(context->stopped_actions, FALSE);
        gtk_action_group_set_sensitive(context->live_actions, TRUE);
        gtk_action_group_set_sensitive(context->dead_actions, FALSE);
        pane_status_changed(context);
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

    g_return_if_fail(gldb_get_status() == GLDB_STATUS_DEAD);
    /* GIOChannel on UNIX is sufficiently light that we may wrap
     * the input pipe in it without forcing us to do everything through
     * it.
     */
    gldb_run(seq++, child_init);
    context->channel = g_io_channel_unix_new(gldb_get_in_pipe());
    context->channel_watch = g_io_add_watch(context->channel, G_IO_IN | G_IO_ERR,
                                            response_callback, context);
    g_child_watch_add(gldb_get_child_pid(), child_exit_callback, context);
    update_status_bar(context, _("Started"));
}

static void stop_action(GtkAction *action, gpointer user_data)
{
    if (gldb_get_status() == GLDB_STATUS_RUNNING)
        gldb_send_async(seq++);
}

static void continue_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    gtk_action_group_set_sensitive(context->stopped_actions, FALSE);
    gtk_action_group_set_sensitive(context->running_actions, TRUE);
    gldb_send_continue(seq++);
    update_status_bar((GldbWindow *) user_data, _("Running"));
}

static void step_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    gtk_action_group_set_sensitive(context->stopped_actions, FALSE);
    gtk_action_group_set_sensitive(context->running_actions, TRUE);
    gldb_send_step(seq++);
    update_status_bar((GldbWindow *) user_data, _("Running"));
}

static void kill_action(GtkAction *action, gpointer user_data)
{
    gldb_send_quit(seq++);
}

static void target_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    gldb_gui_target_dialog_run(context->window);
}

static void attach_gdb_action(GtkAction *action, gpointer user_data)
{
    GldbWindow *context;
    GError *error = NULL;
    gchar *argv[6];

    argv[0] = (gchar *) "xterm";
    argv[1] = (gchar *) "-e";
    argv[2] = (gchar *) "gdb";
    argv[3] = (gchar *) "-p";
    bugle_asprintf(&argv[4], "%lu", (unsigned long) gldb_get_child_pid());
    argv[5] = NULL;

    context = (GldbWindow *) user_data;

    if (!g_spawn_async(NULL,        /* cwd */
                       argv,
                       NULL,        /* environment */
                       G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                       NULL,        /* child setup */
                       NULL,        /* user data */
                       NULL,        /* child pid */
                       &error))
    {
        GtkWidget *dialog;
        dialog = gtk_message_dialog_new(GTK_WINDOW(context->window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("Error launching gdb: %s"),
                                        error->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
    }
    free(argv[4]);
}

#if HAVE_GTK2_6
static void about_action(GtkAction *action, gpointer user_data)
{
    /* static const char *authors[] = {"Bruce Merry <bmerry@users.sourceforge.net>", NULL}; */
    static const char license[] =
        "This program is free software; you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation; version 2.\n\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program; if not, write to the Free Software\n"
        "Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA";
    GldbWindow *context;

    context = (GldbWindow *) user_data;
    gtk_show_about_dialog(GTK_WINDOW(context->window),
                          "title", _("About gldb-gui"),
                          "program-name", "gldb-gui",
                          /* "authors", authors, */
                          "comments", "An open-source OpenGL debugger",
                          "copyright", "2004-2007 Bruce Merry",
                          "license", license,
                          "version", PACKAGE_VERSION,
                          "website", "http://www.opengl.org/sdk/tools/BuGLe/",
                          NULL);
}
#endif

static const gchar *ui_desc =
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Quit' />"
"    </menu>"
"    <menu action='OptionsMenu'>"
"      <menuitem action='Target' />"
"    </menu>"
"    <menu action='RunMenu'>"
"      <menuitem action='Run' />"
"      <menuitem action='Stop' />"
"      <menuitem action='Continue' />"
"      <menuitem action='Step' />"
"      <menuitem action='Kill' />"
"      <separator />"
"      <menuitem action='AttachGDB' />"
"    </menu>"
#if HAVE_GTK2_6
"    <menu action='HelpMenu'>"
"      <menuitem action='About' />"
"    </menu>"
#endif
"  </menubar>"
"</ui>";

static GtkActionEntry action_desc[] =
{
    { "FileMenu", NULL, "_File", NULL, NULL, NULL },
    { "Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q", NULL, G_CALLBACK(quit_action) },

    { "OptionsMenu", NULL, "_Options", NULL, NULL, NULL },
    { "Target", NULL, "_Target", NULL, NULL, G_CALLBACK(target_action) },

    { "RunMenu", NULL, "_Run", NULL, NULL, NULL },

#if HAVE_GTK2_6
    { "HelpMenu", NULL, "_Help", NULL, NULL, NULL },
    { "About", NULL, "_About", NULL, NULL, G_CALLBACK(about_action) }
#endif
};

static GtkActionEntry running_action_desc[] =
{
    { "Stop", NULL, "_Stop", "<control>Break", NULL, G_CALLBACK(stop_action) },
};

static GtkActionEntry stopped_action_desc[] =
{
    { "Continue", NULL, "_Continue", "<control>F9", NULL, G_CALLBACK(continue_action) },
    { "Step", NULL, "_Step", "F8", NULL, G_CALLBACK(step_action) },
    { "Kill", NULL, "_Kill", "<control>F2", NULL, G_CALLBACK(kill_action) }
};

static GtkActionEntry live_action_desc[] =
{
    { "AttachGDB", NULL, "Attach _GDB", NULL, NULL, G_CALLBACK(attach_gdb_action) }
};

static GtkActionEntry dead_action_desc[] =
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
    context->running_actions = gtk_action_group_new("RunningActions");
    gtk_action_group_add_actions(context->running_actions, running_action_desc,
                                 G_N_ELEMENTS(running_action_desc), context);
    context->stopped_actions = gtk_action_group_new("StoppedActions");
    gtk_action_group_add_actions(context->stopped_actions, stopped_action_desc,
                                 G_N_ELEMENTS(stopped_action_desc), context);
    context->live_actions = gtk_action_group_new("LiveActions");
    gtk_action_group_add_actions(context->live_actions, live_action_desc,
                                 G_N_ELEMENTS(live_action_desc), context);
    context->dead_actions = gtk_action_group_new("DeadActions");
    gtk_action_group_add_actions(context->dead_actions, dead_action_desc,
                                 G_N_ELEMENTS(dead_action_desc), context);
    gtk_action_group_set_sensitive(context->running_actions, FALSE);
    gtk_action_group_set_sensitive(context->stopped_actions, FALSE);
    gtk_action_group_set_sensitive(context->live_actions, FALSE);

    ui = gtk_ui_manager_new();
    gtk_ui_manager_set_add_tearoffs(ui, TRUE);
    gtk_ui_manager_insert_action_group(ui, context->actions, 0);
    gtk_ui_manager_insert_action_group(ui, context->running_actions, 0);
    gtk_ui_manager_insert_action_group(ui, context->stopped_actions, 0);
    gtk_ui_manager_insert_action_group(ui, context->live_actions, 0);
    gtk_ui_manager_insert_action_group(ui, context->dead_actions, 0);
    if (!gtk_ui_manager_add_ui_from_string(ui, ui_desc, strlen(ui_desc), &error))
    {
        g_message(_("Failed to create UI: %s"), error->message);
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
    /* Statusbar must be built early, because it is passed to image viewers */
    context->statusbar = gtk_statusbar_new();
    context->statusbar_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(context->statusbar), _("Program status"));
    gtk_statusbar_push(GTK_STATUSBAR(context->statusbar), context->statusbar_context_id, _("Not running"));

    context->panes = g_ptr_array_new();
    main_window_add_pane(context, _("State"), gldb_state_pane_new());
#if HAVE_GTKGLEXT
    main_window_add_pane(context, _("Textures"),
                         gldb_texture_pane_new(GTK_STATUSBAR(context->statusbar),
                                               context->statusbar_context_id));
    main_window_add_pane(context, _("Framebuffers"),
                         gldb_framebuffer_pane_new(GTK_STATUSBAR(context->statusbar),
                                                   context->statusbar_context_id));
#endif
    main_window_add_pane(context, _("Shaders"), gldb_shader_pane_new());
    main_window_add_pane(context, _("Backtrace"), gldb_backtrace_pane_new());
    main_window_add_pane(context, _("Breakpoints"), gldb_breakpoint_pane_new());

    gtk_box_pack_start(GTK_BOX(vbox), context->notebook, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(context->notebook), "switch-page",
                     G_CALLBACK(notebook_switch_page), context);

    gtk_box_pack_start(GTK_BOX(vbox), context->statusbar, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(context->window), vbox);
    gtk_window_set_default_size(GTK_WINDOW(context->window), 640, 480);
    gtk_window_add_accel_group (GTK_WINDOW(context->window),
                                gtk_ui_manager_get_accel_group(ui));

    gtk_widget_show_all(context->window);
}

static void unref_helper(gpointer pane, gpointer data)
{
    g_object_unref(G_OBJECT(pane));
}

int main(int argc, char **argv)
{
    GldbWindow context;

    gtk_init(&argc, &argv);
    wait_cursor = gdk_cursor_new(GDK_WATCH);
#if HAVE_GTKGLEXT
    gtk_gl_init(&argc, &argv);
    gldb_gui_image_initialise();
#endif
    if (argc < 2)
    {
        fputs("Usage: gldb-gui <program> [<arguments>]\n", stderr);
        return 1;
    }
    gldb_initialise(argc, (const char * const *) argv);
    bugle_list_init(&response_handlers, true);

    memset(&context, 0, sizeof(context));
    build_main_window(&context);

    gtk_main();

    gldb_shutdown();
#if HAVE_GTKGLEXT
    gldb_gui_image_shutdown();
#endif
    g_ptr_array_foreach(context.panes, unref_helper, NULL);
    g_ptr_array_free(context.panes, TRUE);
    bugle_list_clear(&response_handlers);
    gdk_cursor_unref(wait_cursor);
    return 0;
}
