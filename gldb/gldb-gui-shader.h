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

/* Shader pane. */

#ifndef BUGLE_GLDB_GLDB_GUI_SHADER_H
#define BUGLE_GLDB_GLDB_GUI_SHADER_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <gtk/gtk.h>
#include <glib.h>
#include "gldb-gui.h"

typedef struct _GldbShaderPane GldbShaderPane;
typedef struct _GldbShaderPaneClass GldbShaderPaneClass;

#define GLDB_SHADER_PANE_TYPE (gldb_shader_pane_get_type())
#define GLDB_SHADER_PANE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GLDB_SHADER_PANE_TYPE, GldbShaderPane))
#define GLDB_SHADER_PANE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GLDB_SHADER_PANE_TYPE, GldbShaderPaneClass))
#define GLDB_IS_SHADER_PANE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GLDB_SHADER_PANE_TYPE))
#define GLDB_IS_SHADER_PANE_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), GLDB_SHADER_PANE_TYPE))
#define GLDB_SHADER_PANE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GLDB_SHADER_PANE_TYPE, GldbShaderPaneClass))
GType gldb_shader_pane_get_type(void);

GldbPane *gldb_shader_pane_new(void);

#endif /* !BUGLE_GLDB_GLDB_GUI_SHADER_H */
