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

#ifndef BUGLE_GLDB_GLDB_GUI_BACKTRACE_H
#define BUGLE_GLDB_GLDB_GUI_BACKTRACE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include "gldb-gui.h"

typedef struct _GldbBacktracePane GldbBacktracePane;
typedef struct _GldbBacktracePaneClass GldbBacktracePaneClass;

#define GLDB_BACKTRACE_PANE_TYPE (gldb_backtrace_pane_get_type())
#define GLDB_BACKTRACE_PANE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GLDB_BACKTRACE_PANE_TYPE, GldbBacktracePane))
#define GLDB_BACKTRACE_PANE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GLDB_BACKTRACE_PANE_TYPE, GldbBacktracePaneClass))
#define GLDB_IS_BACKTRACE_PANE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GLDB_BACKTRACE_PANE_TYPE))
#define GLDB_IS_BACKTRACE_PANE_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), GLDB_BACKTRACE_PANE_TYPE))
#define GLDB_BACKTRACE_PANE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GLDB_BACKTRACE_PANE_TYPE, GldbBacktracePaneClass))
GType gldb_backtrace_pane_get_type(void);

GldbPane *gldb_backtrace_pane_new(void);

#endif /* !BUGLE_GLDB_GLDB_GUI_BACKTRACE_H */
