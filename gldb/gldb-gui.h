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

/* Utility functions that are shared amongst gldb-gui panes. */

#ifndef BUGLE_GLDB_GLDB_GUI_H
#define BUGLE_GLDB_GLDB_GUI_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include "gldb/gldb-common.h"

typedef struct _GldbPane GldbPane;
typedef struct _GldbPaneClass GldbPaneClass;

struct _GldbPane
{
    GObject parent;

    /* protected */
    gboolean dirty;
    GtkWidget *top_widget;
};

struct _GldbPaneClass
{
    GObjectClass parent;
    /* class members */
    void (*do_real_update)(GldbPane *self);
    void (*do_status_changed)(GldbPane *self, gldb_status status);
};

#define GLDB_PANE_TYPE (gldb_pane_get_type())
#define GLDB_PANE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GLDB_PANE_TYPE, GldbPane))
#define GLDB_PANE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GLDB_PANE_TYPE, GldbPaneClass))
#define GLDB_IS_PANE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GLDB_PANE_TYPE))
#define GLDB_IS_PANE_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), GLDB_PANE_TYPE))
#define GLDB_PANE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GLDB_PANE_TYPE, GldbPaneClass))
GType gldb_pane_get_type(void);

/* Checks if dirty, and if so, invokes the virtual update method */
void gldb_pane_update(GldbPane *);
/* Marks the pane as dirty */
void gldb_pane_invalidate(GldbPane *);
/* Returns the top-level widget associated with the pane. */
GtkWidget *gldb_pane_get_widget(GldbPane *);
/* Sets the top-level widget associated with the pane. Should only be
 * called by the subclasses */
void gldb_pane_set_widget(GldbPane *self, GtkWidget *widget);
/* Notify the pane that the program has started/stopped/quit/etc */
void gldb_pane_status_changed(GldbPane *self, gldb_status new_status);

/* Exported functions for panes. Refer to the source for documentation */
void gldb_gui_combo_box_get_old(GtkComboBox *box, GValue *save, ...);
void gldb_gui_combo_box_restore_old(GtkComboBox *box, GValue *save, ...);

guint32 gldb_gui_set_response_handler(gboolean (*callback)(gldb_response *r, gpointer user_data),
                                      gpointer user_data);

#endif /* !BUGLE_GLDB_GLDB_GUI_H */
