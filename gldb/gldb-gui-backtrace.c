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

/* Backtrace pane */

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
#include "common/safemem.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-gui.h"
#include "gldb/gldb-gui-backtrace.h"

static void gldb_backtrace_pane_real_update(GldbPane *self)
{
    gchar *argv[4];
    gint in, out;
    GError *error = NULL;
    GIOChannel *inf, *outf;
    gchar *line;
    gsize terminator;
    const gchar *cmds = "set width 0\nset height 0\nbacktrace\nquit\n";
    GldbBacktracePane *pane;

    pane = GLDB_BACKTRACE_PANE(self);

    gtk_list_store_clear(pane->backtrace_store);
    argv[0] = g_strdup("gdb");
    argv[1] = g_strdup(gldb_get_program());
    bugle_asprintf(&argv[2], "%lu", (unsigned long) gldb_get_child_pid());
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

        while (g_io_channel_read_line(inf, &line, NULL, &terminator, &error) == G_IO_STATUS_NORMAL
               && line != NULL)
            if (line[0] == '#')
            {
                GtkTreeIter iter;

                line[terminator] = '\0';
                gtk_list_store_append(pane->backtrace_store, &iter);
                gtk_list_store_set(pane->backtrace_store, &iter, 0, line, -1);
                g_free(line);
            }
        g_io_channel_unref(inf);
        g_io_channel_unref(outf);
    }
    g_free(argv[0]);
    g_free(argv[1]);
    free(argv[2]);
}

GldbPane *gldb_backtrace_pane_new(void)
{
    GldbBacktracePane *pane;
    GtkWidget *view, *scrolled;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;

    pane = GLDB_BACKTRACE_PANE(g_object_new(GLDB_BACKTRACE_PANE_TYPE, NULL));
    pane->backtrace_store = gtk_list_store_new(1, G_TYPE_STRING);
    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Call"),
                                                      cell,
                                                      "text", 0,
                                                      NULL);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pane->backtrace_store));
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), false);
    gtk_widget_show(view);
    g_object_unref(G_OBJECT(pane->backtrace_store)); /* So that it dies with the view */

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), view);

    gldb_pane_set_widget(GLDB_PANE(pane), scrolled);
    return GLDB_PANE(pane);
}

/* GObject stuff */

static void gldb_backtrace_pane_class_init(GldbBacktracePaneClass *klass)
{
    GldbPaneClass *pane_class = GLDB_PANE_CLASS(klass);

    pane_class->do_real_update = gldb_backtrace_pane_real_update;
}

static void gldb_backtrace_pane_init(GldbBacktracePane *self, gpointer g_class)
{
    self->backtrace_store = NULL;
}

GType gldb_backtrace_pane_get_type(void)
{
    static GType type = 0;
    if (type == 0)
    {
        static const GTypeInfo info =
        {
            sizeof(GldbBacktracePaneClass),
            NULL,                       /* base_init */
            NULL,                       /* base_finalize */
            (GClassInitFunc) gldb_backtrace_pane_class_init,
            NULL,                       /* class_finalize */
            NULL,                       /* class_data */
            sizeof(GldbBacktracePane),
            0,                          /* n_preallocs */
            (GInstanceInitFunc) gldb_backtrace_pane_init,
            NULL                        /* value table */
        };
        type = g_type_register_static(GLDB_PANE_TYPE,
                                      "GldbBacktracePaneType",
                                      &info, 0);
    }
    return type;
}
