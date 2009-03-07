/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009  Bruce Merry
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

#ifndef BUGLE_GLDB_GLDB_GUI_BUFFER_H
#define BUGLE_GLDB_GLDB_GUI_BUFFER_H

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include "gldb/gldb-gui.h"

typedef struct _GldbBufferPane GldbBufferPane;
typedef struct _GldbBufferPaneClass GldbBufferPaneClass;

#define GLDB_BUFFER_PANE_TYPE (gldb_buffer_pane_get_type())
#define GLDB_BUFFER_PANE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GLDB_BUFFER_PANE_TYPE, GldbBufferPane))
#define GLDB_BUFFER_PANE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GLDB_BUFFER_PANE_TYPE, GldbBufferPaneClass))
#define GLDB_IS_BUFFER_PANE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GLDB_BUFFER_PANE_TYPE))
#define GLDB_IS_BUFFER_PANE_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), GLDB_BUFFER_PANE_TYPE))
#define GLDB_BUFFER_PANE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GLDB_BUFFER_PANE_TYPE, GldbBufferPaneClass))
GType gldb_buffer_pane_get_type(void);

GldbPane *gldb_buffer_pane_new(void);

#endif /* !BUGLE_GLDB_GLDB_GUI_BUFFER_H */
