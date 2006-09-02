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

/* Functions in gldb-gui that deal with the state viewer. Other
 * (non-GUI) state-handling is done in gldb-common.c
 */

#ifndef BUGLE_GLDB_GLDB_GUI_STATE_H
#define BUGLE_GLDB_GLDB_GUI_STATE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <gtk/gtk.h>

typedef struct
{
    gboolean dirty;
    GtkWidget *page;
    GtkWidget *only_selected, *only_modified;
    GtkTreeStore *state_store;
    GtkTreeModel *state_filter;  /* Filter that shows only chosen state */
} GldbWindowState;

void state_mark_dirty(GldbWindowState *state);
void state_page_new(GldbWindowState *state, GtkNotebook *notebook);

#endif /* !BUGLE_GLDB_GLDB_GUI_STATE_H */
