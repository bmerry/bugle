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

/* Functions in gldb-gui that deal with the display of images such as
 * textures and framebuffers.
 */

#ifndef BUGLE_GLDB_GLDB_GUI_IMAGE_H
#define BUGLE_GLDB_GLDB_GUI_IMAGE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#if HAVE_GTKGLEXT

/* FIXME: Not sure if this is the correct definition of GETTEXT_PACKAGE... */
#ifndef GETTEXT_PACKAGE
# define GETTEXT_PACKAGE "bugle00"
#endif
#include <GL/gl.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "common/safemem.h"
#include "common/bool.h"
#include "src/names.h"
#include "gldb/gldb-common.h"
#include "gldb/gldb-channels.h"

typedef struct
{
    int width;
    int height;
    uint32_t channels;
    GLfloat *pixels;
} GldbGuiImage;

typedef struct
{
    GldbGuiImage *current;
    int texture_width;
    int texture_height;
    GLint max_viewport_dims[2];

    GtkWidget *draw, *alignment;
    GtkWidget *zoom;
    GtkToolItem *copy, *zoom_in, *zoom_out, *zoom_100, *zoom_fit;

    GtkStatusbar *statusbar;
    guint statusbar_context_id;
    gint pixel_status_id;
} GldbGuiImageViewer;

GldbGuiImageViewer *gldb_gui_image_viewer_new(GtkStatusbar *statusbar,
                                              guint statusbar_context_id);

#endif /* HAVE_GTKGLEXT */
#endif /* !BUGLE_GLDB_GLDB_GUI_IMAGE_H */
