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

/* Functions in gldb-gui that deal with the framebuffer viewer. Generic image
 * handling code is in gldb-gui-image.c
 */

#ifndef BUGLE_GLDB_GLDB_GUI_FRAMEBUFFER_H
#define BUGLE_GLDB_GLDB_GUI_FRAMEBUFFER_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#if HAVE_GTKGLEXT

#include <gtk/gtk.h>
#include <glib.h>
#include "gldb-gui.h"
#include "gldb-gui-image.h"

typedef struct _GldbFramebufferPane GldbFramebufferPane;
typedef struct _GldbFramebufferPaneClass GldbFramebufferPaneClass;

struct _GldbFramebufferPane
{
    GldbPane parent;

    /* private */
    gboolean dirty;
    GtkWidget *top_widget;
    GtkWidget *id, *buffer;

    GldbGuiImage active;
    GldbGuiImageViewer *viewer;
};

struct _GldbFramebufferPaneClass
{
    GldbPaneClass parent;
};

#define GLDB_FRAMEBUFFER_PANE_TYPE (gldb_framebuffer_pane_get_type())
#define GLDB_FRAMEBUFFER_PANE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GLDB_FRAMEBUFFER_PANE_TYPE, GldbFramebufferPane))
#define GLDB_FRAMEBUFFER_PANE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GLDB_FRAMEBUFFER_PANE_TYPE, GldbFramebufferPaneClass))
#define GLDB_IS_FRAMEBUFFER_PANE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GLDB_FRAMEBUFFER_PANE_TYPE))
#define GLDB_IS_FRAMEBUFFER_PANE_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), GLDB_FRAMEBUFFER_PANE_TYPE))
#define GLDB_FRAMEBUFFER_PANE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GLDB_FRAMEBUFFER_PANE_TYPE, GldbFramebufferPaneClass))
GType gldb_framebuffer_pane_get_type(void);

GldbPane *gldb_framebuffer_pane_new(GtkStatusbar *statusbar,
                                    guint statusbar_context_id);

#endif /* HAVE_GTKGLEXT */
#endif /* !BUGLE_GLDB_GLDB_GUI_FRAMEBUFFER_H */
