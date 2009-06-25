/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007, 2009  Bruce Merry
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

#ifndef BUGLE_GLDB_GLDB_GUI_TARGET_H
#define BUGLE_GLDB_GLDB_GUI_TARGET_H

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <gtk/gtk.h>

/* Display the target dialog and gather responses
 * Returns BUGLE_TRUE if the user accepted the dialog.
 */
bugle_bool gldb_gui_target_dialog_run(GtkWidget *parent);

#endif
