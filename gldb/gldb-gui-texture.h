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

#ifndef BUGLE_GLDB_GLDB_GUI_TEXTURE_H
#define BUGLE_GLDB_GLDB_GUI_TEXTURE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#if HAVE_GTKGLEXT

#include <gtk/gtk.h>
#include <glib.h>
#include "gldb-gui.h"
#include "gldb-gui-image.h"

typedef struct _GldbTexturePane GldbTexturePane;
typedef struct _GldbTexturePaneClass GldbTexturePaneClass;

struct _GldbTexturePane
{
    GldbPane parent;

    /* private */
    gboolean dirty;
    GtkWidget *top_widget;
    GtkWidget *id, *level;

    GldbGuiImageViewer *viewer;
    GldbGuiImage active;           /* visible on the screen */
    GldbGuiImage progressive;      /* currently being assembled */
};

struct _GldbTexturePaneClass
{
    GldbPaneClass parent;
};

#define GLDB_TEXTURE_PANE_TYPE (gldb_texture_pane_get_type())
#define GLDB_TEXTURE_PANE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GLDB_TEXTURE_PANE_TYPE, GldbTexturePane))
#define GLDB_TEXTURE_PANE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GLDB_TEXTURE_PANE_TYPE, GldbTexturePaneClass))
#define GLDB_IS_TEXTURE_PANE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GLDB_TEXTURE_PANE_TYPE))
#define GLDB_IS_TEXTURE_PANE_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), GLDB_TEXTURE_PANE_TYPE))
#define GLDB_TEXTURE_PANE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GLDB_TEXTURE_PANE_TYPE, GldbTexturePaneClass))
GType gldb_texture_pane_get_type(void);

GldbPane *gldb_texture_pane_new(GtkStatusbar *statusbar,
                                guint statusbar_context_id);

#endif /* HAVE_GTKGLEXT */
#endif /* !BUGLE_GLDB_GLDB_GUI_TEXTURE_H */
