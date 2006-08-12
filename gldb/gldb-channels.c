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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <inttypes.h>
#include "common/bool.h"
#include "gldb/gldb-channels.h"

gldb_channel_table_entry gldb_channel_table[] =
{
    /* Single channels. Note: these are in the canonical order that they
     * will appear in a readback. In particular, luminance must appear
     * before alpha.
     */
    { GLDB_CHANNEL_LUMINANCE, GL_LUMINANCE, GL_LUMINANCE, GL_TEXTURE_LUMINANCE_SIZE, "L", "luminance" },
    { GLDB_CHANNEL_INTENSITY, GL_INTENSITY, GL_INTENSITY, GL_TEXTURE_INTENSITY_SIZE, "I", "intensity" },
    { GLDB_CHANNEL_RED, 0, 0, GL_TEXTURE_RED_SIZE, "R", "red" },
    { GLDB_CHANNEL_GREEN, 0, 0, GL_TEXTURE_GREEN_SIZE, "G", "green" },
    { GLDB_CHANNEL_BLUE, 0, 0, GL_TEXTURE_BLUE_SIZE, "B", "blue" },
    { GLDB_CHANNEL_ALPHA, GL_ALPHA, GL_LUMINANCE, GL_TEXTURE_ALPHA_SIZE, "A", "alpha" },
#ifdef GL_ARB_depth_texture
    { GLDB_CHANNEL_DEPTH, GL_DEPTH_COMPONENT, GL_LUMINANCE, GL_TEXTURE_DEPTH_SIZE, "D", "depth" },
#else
    { GLDB_CHANNEL_DEPTH, GL_DEPTH_COMPONENT, GL_LUMINANCE, 0, "D", "depth" },
#endif
    { GLDB_CHANNEL_STENCIL, GL_STENCIL_INDEX, GL_LUMINANCE, 0, "S", "stencil" },
    { GLDB_CHANNEL_COLOR_INDEX, GL_COLOR_INDEX, GL_LUMINANCE, 0, "C", "color-index" },

    /* Composites */
    { GLDB_CHANNEL_RGB, GL_RGB, GL_RGB, 0, "RGB", NULL },
    { GLDB_CHANNEL_RGBA, GL_RGBA, GL_RGBA, 0, "RGBA", NULL },
    { GLDB_CHANNEL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, 0, NULL, NULL },
#ifdef GL_NV_packed_depth_stencil
    { GLDB_CHANNEL_DEPTH_STENCIL, GL_DEPTH_STENCIL_NV, 0, 0, NULL, NULL },
#endif

    { 0, GL_NONE, GL_NONE, 0, NULL, NULL }
};

uint32_t gldb_channel_get_query_channels(uint32_t channels)
{
    int i;

    if (!channels) return 0; /* Dubious query to begin with */
    for (i = 0; gldb_channel_table[i].channel; i++)
        if (!(channels & ~gldb_channel_table[i].channel))
            return gldb_channel_table[i].channel;
    return 0;
}

GLenum gldb_channel_get_query_token(uint32_t channels)
{
    int i;

    if (!channels) return GL_NONE; /* Dubious query to begin with */
    for (i = 0; gldb_channel_table[i].channel; i++)
        if (channels == gldb_channel_table[i].channel)
            return gldb_channel_table[i].query_token;
    return GL_NONE;
}

GLenum gldb_channel_get_display_token(uint32_t channels)
{
    int i;

    if (!channels) return GL_NONE; /* Dubious query to begin with */
    for (i = 0; gldb_channel_table[i].channel; i++)
        if (channels == gldb_channel_table[i].channel)
            return gldb_channel_table[i].display_token;
    return GL_NONE;
}

const char *gldb_channel_get_abbr(uint32_t channels)
{
    int i;

    if (!channels) return NULL; /* Dubious query to begin with */
    for (i = 0; gldb_channel_table[i].channel; i++)
        if (channels == gldb_channel_table[i].channel)
            return gldb_channel_table[i].abbr;
    return NULL;
}

int gldb_channel_count(uint32_t channels)
{
    int ans = 0;
    while (channels)
    {
        ans++;
        channels &= channels - 1;
    }
    return ans;
}
