/*  BuGLe an OpenGL debugging tool
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
#include <stdlib.h>
#include <stdio.h>
#include "common/linkedlist.h"
#include "common/hashtable.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-gui.h"
#include "gldb/gldb-gui-backtrace.h"
#include "xalloc.h"
#include "xstrndup.h"
#include "xvasprintf.h"

/* Functions and structures to parse GDB/MI output. Split this out of
 * gldb-gui one day.
 */

typedef enum
{
    GLDB_GDB_VALUE_CONST,
    GLDB_GDB_VALUE_TUPLE,
    GLDB_GDB_VALUE_LIST
} GldbGdbValueType;

typedef enum
{
    GLDB_GDB_RESULT_CLASS_DONE,
    GLDB_GDB_RESULT_CLASS_RUNNING,
    GLDB_GDB_RESULT_CLASS_CONNECTED,
    GLDB_GDB_RESULT_CLASS_ERROR,
    GLDB_GDB_RESULT_CLASS_EXIT
} GldbGdbResultClass;

enum
{
    GLDB_GDB_ERROR_PARSE
};

typedef struct
{
    GldbGdbValueType type;

    char *const_value;
    linked_list list_value;   /* currently only a list of GldbGdbValue */
    hash_table tuple_value;   /* table of name => GldbGdbValue */
} GldbGdbValue;

typedef struct
{
    GldbGdbResultClass result_class;
    hash_table results;
} GldbGdbResultRecord;

static GQuark gldb_gdb_error_quark(void)
{
    static GQuark q = 0;
    if (q == 0)
        q = g_quark_from_static_string("gldb-gdb-error-quark");
    return q;
}

#define GLDB_GDB_ERROR (gldb_gdb_error_quark())

static void gldb_gdb_value_free(GldbGdbValue *value)
{
    switch (value->type)
    {
    case GLDB_GDB_VALUE_CONST:
        free(value->const_value);
        break;
    case GLDB_GDB_VALUE_LIST:
        bugle_list_clear(&value->list_value);
        break;
    case GLDB_GDB_VALUE_TUPLE:
        bugle_hash_clear(&value->tuple_value);
        break;
    }
    free(value);
}

/* Returns the value from a tuple with the specified name. The value must
 * exist and be a string, otherwise NULL is returned.
 */
static char *gldb_gdb_value_get_string_field(GldbGdbValue *tuple, const char *name)
{
    GldbGdbValue *v;

    g_return_val_if_fail(tuple->type == GLDB_GDB_VALUE_TUPLE, NULL);
    v = (GldbGdbValue *) bugle_hash_get(&tuple->tuple_value, name);
    if (v && v->type == GLDB_GDB_VALUE_CONST)
        return v->const_value;
    else
        return NULL;
}

static void gldb_gdb_result_record_free(GldbGdbResultRecord *record)
{
    bugle_hash_clear(&record->results);
    free(record);
}

/* Updates the record_ptr to point just beyond the extracted value.
 * If an error occurs, the error is set, NULL is returned and record_ptr
 * is not updated.
 */
static GldbGdbValue *gldb_gdb_value_parse(const char **record_ptr, GError **error)
{
    const char *record, *end;
    GldbGdbValue *v;

    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    record = *record_ptr;
    v = XMALLOC(GldbGdbValue);
    if (record[0] == '"')
    {
        v->type = GLDB_GDB_VALUE_CONST;
        record++;
        end = record;
        while (end[0] != '"')
        {
            if (end[0] == '\\') end += 2;
            else end++;
        }
        if (end[0] != '"')
        {
            free(v);
            g_set_error(error, GLDB_GDB_ERROR, GLDB_GDB_ERROR_PARSE,
                        "Failed to parse value: expected '\"'");
            return NULL;
        }
        v->const_value = xstrndup(record, end - record);
        g_return_val_if_fail(end[0] == '"', NULL);
        record = end + 1;
    }
    else if (record[0] == '{')
    {
        v->type = GLDB_GDB_VALUE_TUPLE;
        bugle_hash_init(&v->tuple_value, (void (*)(void *)) gldb_gdb_value_free);
        if (record[1] != '}')
        {
            do
            {
                char *name;
                GldbGdbValue *value, *old;

                record++;
                end = record;
                while (end[0] != '=' && end[0] != '\0') end++;
                if (end[0] != '=')
                {
                    gldb_gdb_value_free(v);
                    g_set_error(error, GLDB_GDB_ERROR, GLDB_GDB_ERROR_PARSE,
                                "Failed to parse value: expected '='");
                    return NULL;
                }
                name = xstrndup(record, end - record);
                record = end + 1;
                value = gldb_gdb_value_parse(&record, error);
                if (!value)
                {
                    gldb_gdb_value_free(v);
                    return NULL;
                }
                old = (GldbGdbValue *) bugle_hash_get(&v->tuple_value, name);
                if (old) gldb_gdb_value_free(old);
                bugle_hash_set(&v->tuple_value, name, value);
                free(name);
            } while (record[0] == ',');
            if (record[0] != '}')
            {
                gldb_gdb_value_free(v);
                g_set_error(error, GLDB_GDB_ERROR, GLDB_GDB_ERROR_PARSE,
                            "Failed to parse value: expected '}'");
                return NULL;
            }
            record++;
        }
        else
            record += 2;
    }
    else if (record[0] == '[')
    {
        /* If we have a list of name=value pairs, we discard the names.
         * This works for -stack-list-frames, which has frame={...} for
         * each entry.
         */
        v->type = GLDB_GDB_VALUE_LIST;
        bugle_list_init(&v->list_value, (void (*)(void *)) gldb_gdb_value_free);
        if (record[1] != ']')
        {
            do
            {
                GldbGdbValue *value;

                record++;
                if (record[0] != '{' && record[0] != '[' && record[0] != '"')
                {
                    while (record[0] != '=' && record[0] != '\0') record++;
                    if (record[0] != '=')
                    {
                        gldb_gdb_value_free(v);
                        g_set_error(error, GLDB_GDB_ERROR, GLDB_GDB_ERROR_PARSE,
                                    "Failed to parse value: expected '='");
                        return NULL;
                    }
                    record++;
                }
                value = gldb_gdb_value_parse(&record, error);
                if (!value)
                {
                    gldb_gdb_value_free(v);
                    return NULL;
                }
                bugle_list_append(&v->list_value, value);
            } while (record[0] == ',');
            if (record[0] != ']')
            {
                gldb_gdb_value_free(v);
                g_set_error(error, GLDB_GDB_ERROR, GLDB_GDB_ERROR_PARSE,
                            "Failed to parse value: expected ']'");
                return NULL;
            }
            record++;
        }
        else
            record += 2;
    }
    else
    {
        free(v);
        g_set_error(error, GLDB_GDB_ERROR, GLDB_GDB_ERROR_PARSE,
                    "Failed to parse value: expected '\"', '[' or '{'");
        return NULL;
    }

    *record_ptr = record;
    return v;
}

/* Currently ignores everything except result records, returning NULL.
 * Note that passing a non-result record is not an error, but passing
 * a non-parsable result record is. See the GDB docs for the formal
 * description of the format.
 *
 * One day this could probably be better and more robustly written with
 * lex+yacc.
 */
static GldbGdbResultRecord *gldb_gdb_result_record_parse(const char *record, GError **error)
{
    const char *end;
    GldbGdbResultRecord *rr;
    GldbGdbResultClass rc;

    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    /* Skip the token. Be careful not to use locale-dependent functions here */
    while (record[0] >= '0' && record[0] <= '9') record++;
    if (record[0] != '^') return NULL; /* not a result, not an error */
    record++;

    /* Find end of the result class */
    end = record;
    while (end[0] >= 'a' && end[0] <= 'z') end++;
    if (strncmp(record, "done", end - record) == 0)
        rc = GLDB_GDB_RESULT_CLASS_DONE;
    else if (strncmp(record, "running", end - record) == 0)
        rc = GLDB_GDB_RESULT_CLASS_RUNNING;
    else if (strncmp(record, "connected", end - record) == 0)
        rc = GLDB_GDB_RESULT_CLASS_CONNECTED;
    else if (strncmp(record, "error", end - record) == 0)
        rc = GLDB_GDB_RESULT_CLASS_ERROR;
    else if (strncmp(record, "exit", end - record) == 0)
        rc = GLDB_GDB_RESULT_CLASS_EXIT;
    else
    {
        g_set_error(error, GLDB_GDB_ERROR, GLDB_GDB_ERROR_PARSE,
                    "Failed to parse record: invalid result class");
        return NULL;
    }

    rr = XMALLOC(GldbGdbResultRecord);
    rr->result_class = rc;
    bugle_hash_init(&rr->results, (void (*)(void *)) gldb_gdb_value_free);

    /* Extract the results */
    record = end;
    while (record[0] == ',')
    {
        char *name;
        GldbGdbValue *value, *old;

        record++;
        /* Find the = */
        end = record;
        while (end[0] != '=' && end[0] != '\0') end++;
        if (end[0] != '=')
        {
            g_set_error(error, GLDB_GDB_ERROR, GLDB_GDB_ERROR_PARSE,
                        "Failed to parse record: expected '='");
            gldb_gdb_result_record_free(rr);
            return NULL;
        }

        name = xstrndup(record, end - record);
        record = end + 1;
        value = gldb_gdb_value_parse(&record, error);
        if (!value)
        {
            gldb_gdb_result_record_free(rr);
            return NULL;
        }
        old = bugle_hash_get(&rr->results, name);
        if (old) gldb_gdb_value_free(old);
        bugle_hash_set(&rr->results, name, value);
        free(name);
    }
    return rr;
}

/**********************************************************************/

struct _GldbBacktracePane
{
    GldbPane parent;

    /* private */
    gboolean dirty;
    GtkWidget *top_widget;
    GtkListStore *backtrace_store;
};

struct _GldbBacktracePaneClass
{
    GldbPaneClass parent;
};

enum
{
    COLUMN_BACKTRACE_LEVEL,
    COLUMN_BACKTRACE_ADDRESS,
    COLUMN_BACKTRACE_FUNCTION,
    COLUMN_BACKTRACE_LOCATION
};

static void gldb_backtrace_pane_populate(GldbBacktracePane *pane,
                                         gchar *response)
{
    GtkTreeIter iter;
    linked_list_node *l;
    GldbGdbValue *stack;
    GldbGdbValue *frame;
    const char *level, *addr, *func, *file, *line, *from;
    char *location;
    GldbGdbResultRecord *result;
    GError *error = NULL;

    result = gldb_gdb_result_record_parse(response, &error);
    if (error)
    {
        GtkWidget *dialog;
        GtkWidget *top;

        top = gtk_widget_get_toplevel(gldb_pane_get_widget(GLDB_PANE(pane)));
        dialog = gtk_message_dialog_new(GTK_WINDOW(top),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        "%s",
                                        error->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
        return;
    }
    if (!result || result->result_class != GLDB_GDB_RESULT_CLASS_DONE)
    {
        if (result) gldb_gdb_result_record_free(result);
        return;
    }

    stack = (GldbGdbValue *) bugle_hash_get(&result->results, "stack");
    if (!stack || stack->type != GLDB_GDB_VALUE_LIST)
    {
        gldb_gdb_result_record_free(result);
        return;
    }

    gtk_list_store_clear(pane->backtrace_store);
    for (l = bugle_list_head(&stack->list_value); l; l = bugle_list_next(l))
    {
        frame = (GldbGdbValue *) bugle_list_data(l);
        if (!frame || frame->type != GLDB_GDB_VALUE_TUPLE) continue;

        level = gldb_gdb_value_get_string_field(frame, "level");
        addr = gldb_gdb_value_get_string_field(frame, "addr");
        func = gldb_gdb_value_get_string_field(frame, "func");
        file = gldb_gdb_value_get_string_field(frame, "file");
        line = gldb_gdb_value_get_string_field(frame, "line");
        from = gldb_gdb_value_get_string_field(frame, "from");

        if (file && line)
            location = xasprintf("%s:%s", file, line);
        else if (from)
            location = xstrdup(from);
        else
            location = xstrdup("??");

        gtk_list_store_append(pane->backtrace_store, &iter);
        gtk_list_store_set(pane->backtrace_store, &iter,
                           COLUMN_BACKTRACE_LEVEL, level ? level : "??",
                           COLUMN_BACKTRACE_ADDRESS, addr ? addr : "??",
                           COLUMN_BACKTRACE_FUNCTION, func ? func : "??",
                           COLUMN_BACKTRACE_LOCATION, location,
                           -1);
        free(location);
    }

    gldb_gdb_result_record_free(result);
}

/* Uses GDB/MI to query the stack */
static void gldb_backtrace_pane_real_update(GldbPane *self)
{
    gchar *argv[8];
    gint in, out;
    GError *error = NULL;
    GIOChannel *inf, *outf;
    gchar *line;
    gsize terminator;
    const gchar *cmds = "-stack-list-frames\n-gdb-exit\n";
    GldbBacktracePane *pane;

    pane = GLDB_BACKTRACE_PANE(self);
    gtk_list_store_clear(pane->backtrace_store);

    argv[0] = (gchar *) "gdb";
    argv[1] = (gchar *) "-nx";   /* ignore startup files */
    argv[2] = (gchar *) "-nw";   /* no windows */
    argv[3] = (gchar *) "-q";    /* quiet */
    argv[4] = (gchar *) "--interpreter=mi2";
    argv[5] = (gchar *) "-p";
    argv[6] = xasprintf("%lu", (unsigned long) gldb_get_child_pid());
    argv[7] = NULL;
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
        {
            if (line[0] == '^') /* response record */
                gldb_backtrace_pane_populate(pane, line);
        }
        g_io_channel_unref(inf);
        g_io_channel_unref(outf);
    }
    free(argv[6]);
}

void gldb_backtrace_pane_status_changed(GldbPane *self, gldb_status new_status)
{
    GldbBacktracePane *pane;

    pane = GLDB_BACKTRACE_PANE(self);
    if (new_status == GLDB_STATUS_DEAD)
        gtk_list_store_clear(GTK_LIST_STORE(pane->backtrace_store));
}

GldbPane *gldb_backtrace_pane_new(void)
{
    GldbBacktracePane *pane;
    GtkWidget *view, *scrolled;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;

    pane = GLDB_BACKTRACE_PANE(g_object_new(GLDB_BACKTRACE_PANE_TYPE, NULL));
    pane->backtrace_store = gtk_list_store_new(4,
                                               G_TYPE_STRING,
                                               G_TYPE_STRING,
                                               G_TYPE_STRING,
                                               G_TYPE_STRING);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pane->backtrace_store));

    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Frame"),
                                                      cell,
                                                      "text", COLUMN_BACKTRACE_LEVEL,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Address"),
                                                      cell,
                                                      "text", COLUMN_BACKTRACE_ADDRESS,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Function"),
                                                      cell,
                                                      "text", COLUMN_BACKTRACE_FUNCTION,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    cell = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Location"),
                                                      cell,
                                                      "text", COLUMN_BACKTRACE_LOCATION,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

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
    GldbPaneClass *pane_class;

    pane_class = GLDB_PANE_CLASS(klass);
    pane_class->do_real_update = gldb_backtrace_pane_real_update;
    pane_class->do_status_changed = gldb_backtrace_pane_status_changed;
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
