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

typedef enum
{
    GLDB_GUI_IMAGE_TYPE_2D,   /* also includes framebuffer */
    GLDB_GUI_IMAGE_TYPE_3D,
    GLDB_GUI_IMAGE_TYPE_CUBE_MAP
} GldbGuiImageType;

/* A simple 2D image. This is
 * - a level of a 2D texture
 * - a face of a level of a cube-map
 * - a level of a 3D texture.
 * - a framebuffer
 */
typedef struct
{
    int width;
    int height;
    uint32_t channels;
    GLfloat *pixels;
    bool owns_pixels;
} GldbGuiImagePlane;

/* A bag of 2D images that occur at the same level of a texture. This is
 * - a level of a 2D texture (nplanes = 1)
 * - a level of a cube-map (nplanes = 6)
 * - a level of a 3D texture (nplanes = depth)
 * - a framebuffer (nplanes = 1)
 */
typedef struct
{
    int nplanes;
    GldbGuiImagePlane *planes;
} GldbGuiImageLevel;

/* A multiresolution set of levels. For textures, this contains one or
 * more levels. For framebuffers, it currently contains exactly 1, which
 * may one day be extended to cater for multisample framebuffers (I don't
 * even know if it is possible to directly read such a buffer).
 *
 * A GldbGuiImage always corresponds to a single texture, whose type is
 * matched to that of the image. The texture is put in the default
 * texture object.
 */
typedef struct
{
    GldbGuiImageType type;

    int nlevels;
    GldbGuiImageLevel *levels;

    /* Top-right corner of the usable image data. This is needed when
     * displaying NPOT textures or framebuffers on POT-only hardware,
     * where the texture size is rounded up.
     */
    GLfloat s, t, r;
    GLenum texture_target;
} GldbGuiImage;

typedef struct
{
    GldbGuiImage *current;
    int current_level;  /* -1 means "all levels" */
    int current_plane;  /* -1 means all faces for a cube-map */

    GLint max_viewport_dims[2];
    GLenum texture_mag_filter, texture_min_filter;

    GtkWidget *draw, *alignment;
    GtkWidget *zoom;
    GtkToolItem *copy, *zoom_in, *zoom_out, *zoom_100, *zoom_fit;

    GtkStatusbar *statusbar;
    guint statusbar_context_id;
    guint pixel_status_id;

    /* These are optional widgets that are allocated by the respective
     * function.
     */
    GtkWidget *level, *face, *zoffset;
    GtkWidget *mag_filter, *min_filter;
    GtkCellRenderer *min_filter_renderer;
} GldbGuiImageViewer;

GldbGuiImageViewer *gldb_gui_image_viewer_new(GtkStatusbar *statusbar,
                                              guint statusbar_context_id);

/* Creates an add-on level widget to control the level */
GtkWidget *gldb_gui_image_viewer_level_new(GldbGuiImageViewer *viewer);

/* Create add-on widgets to control the face on cube maps or the zoffset
 * on 3D textures. */
GtkWidget *gldb_gui_image_viewer_face_new(GldbGuiImageViewer *viewer);
GtkWidget *gldb_gui_image_viewer_zoffset_new(GldbGuiImageViewer *viewer);

/* Creates add-on widgets to control the filtering */
GtkWidget *gldb_gui_image_viewer_filter_new(GldbGuiImageViewer *viewer, bool mag);

/* Clears the memory allocated to the structure, but not the structure itself */
void gldb_gui_image_plane_clear(GldbGuiImagePlane *plane);
void gldb_gui_image_level_clear(GldbGuiImageLevel *level);
void gldb_gui_image_clear(GldbGuiImage *image);

/* Updates the sensitivity of the zoom combo items and zoom button, and
 * switches to a legal zoom level if the previous level is now illegal.
 */
void gldb_gui_image_viewer_update_zoom(GldbGuiImageViewer *viewer);

/* Rebuilds the levels widget with the appropriate levels for the
 * current texture. A levels widget need not be present.
 */
void gldb_gui_image_viewer_update_levels(GldbGuiImageViewer *viewer);

/* Updates the sensitivity and range of the face and zoffset widgets, if
 * any. This routine may or may not schedule a redraw.
 */
void gldb_gui_image_viewer_update_face_zoffset(GldbGuiImageViewer *viewer);

/* Sets the sensitivity of the mipmapping min-filters. */
void gldb_gui_image_viewer_update_min_filter(GldbGuiImageViewer *viewer,
                                             bool sensitive);


/* Creates all the memory for nlevels levels, each with nplanes planes.
 * The image should be uninitialised i.e., no existing memory will be freed.
 */
void gldb_gui_image_allocate(GldbGuiImage *image, GldbGuiImageType type,
                             int nlevels, int nplanes);

/* General initialisation function; call before all others */
void gldb_gui_image_initialise(void);

/* Corresponding shutdown function */
void gldb_gui_image_shutdown(void);

#endif /* HAVE_GTKGLEXT */
#endif /* !BUGLE_GLDB_GLDB_GUI_IMAGE_H */
