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

/* Assorted tables for managing information about various types of channels
 * (RGBA, luminance, depth etc).
 */

#ifndef BUGLE_GLDB_GLDB_CHANNELS_H
#define BUGLE_GLDB_GLDB_CHANNELS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <inttypes.h>

/* Note: Luminance must appear before alpha, so that LUMINANCE_ALPHA is
 * processed correctly.
 */
#define GLDB_CHANNEL_LUMINANCE 0x0001
#define GLDB_CHANNEL_INTENSITY 0x0002
#define GLDB_CHANNEL_RED       0x0004
#define GLDB_CHANNEL_GREEN     0x0008
#define GLDB_CHANNEL_BLUE      0x0010
#define GLDB_CHANNEL_ALPHA     0x0020
#define GLDB_CHANNEL_DEPTH     0x0040
#define GLDB_CHANNEL_STENCIL   0x0080
#define GLDB_CHANNEL_COLOR_INDEX 0x0100

#define GLDB_CHANNEL_RGB (GLDB_CHANNEL_RED | GLDB_CHANNEL_GREEN | GLDB_CHANNEL_BLUE)
#define GLDB_CHANNEL_RGBA (GLDB_CHANNEL_RGB | GLDB_CHANNEL_ALPHA)
#define GLDB_CHANNEL_LUMINANCE_ALPHA (GLDB_CHANNEL_LUMINANCE | GLDB_CHANNEL_ALPHA)
#define GLDB_CHANNEL_DEPTH_STENCIL (GLDB_CHANNEL_DEPTH | GLDB_CHANNEL_STENCIL)

/* Defines the relationship between various uses of a channel.
 * For composites (e.g. RGB) only the first two fields are guaranteed to be present.
 */
typedef struct
{
    uint32_t channel;
    GLenum query_token;         /* Name for glGetTexImage / glReadPixels */
    GLenum display_token;       /* Equivalent token to get colour image */
    GLenum texture_size_token;  /* GL_TEXTURE_RED_SIZE etc */
    const char *abbr;           /* Abbreviation e.g. R for red */
    const char *text;           /* Full name e.g. "red" for red */
} gldb_channel_table_entry;

/* Table of channel table entries. The end is indicated by a field with
 * all zeros.
 */
extern gldb_channel_table_entry gldb_channel_table[];

/* Returns the most appropriate channelset that is a superset of the given
 * set. "Appropriate" means that there is a GL token to query with. If there
 * is no super-set, returns 0.
 */
uint32_t gldb_channel_get_query_channels(uint32_t channels);

/* Returns the OpenGL token to pass to glGetTexImage / glReadPixels to
 * retrieve the given set of channels. It will not automatically expand
 * the set; use gldb_channel_get_query_channel to do that. If no token
 * is found, returns GL_NONE.
 */
GLenum gldb_channel_get_query_token(uint32_t channels);

/* Similar to the above, but returns the abbreviation, or NULL */
const char *gldb_channel_get_abbr(uint32_t channels);

/* Convenience function to count the number of channels in a set. */
int gldb_channel_count(uint32_t channels);

#endif /* BUGLE_GLDB_GLDB_CHANNELS_H */
