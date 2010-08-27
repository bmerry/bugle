/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2006-2010  Bruce Merry
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

/* Uses PBOs with a number of functions that take PBOs, together with the logger.
 * If we don't handle this right, it can crash the app.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "test.h"
#include <GL/glew.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

static GLuint pbo_ids[2];
static GLvoid *offset = (GLvoid *) (size_t) 4;

static bugle_bool check_support(void)
{
    if (!GLEW_ARB_pixel_buffer_object)
    {
        test_skipped("ARB_pixel_buffer_object required");
        return BUGLE_FALSE;
    }
    else
        return BUGLE_TRUE;
}

static void pbo_setup(void)
{
    GLsizei pbo_size = 512 * 512 * 4 + 4;

    if (!GLEW_ARB_pixel_buffer_object) return;

    glGenBuffersARB(2, pbo_ids);
    test_log_printf("trace\\.call: glGenBuffersARB\\(2, %p -> { %u, %u }\\)\n", pbo_ids, (unsigned int) pbo_ids[0], (unsigned int) pbo_ids[1]);
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, pbo_ids[0]);
    test_log_printf("trace\\.call: glBindBufferARB\\(GL_PIXEL_PACK_BUFFER(_EXT|_ARB)?, %u\\)\n", pbo_ids[0]);
    glBufferDataARB(GL_PIXEL_PACK_BUFFER_EXT, pbo_size, NULL, GL_DYNAMIC_READ_ARB);
    test_log_printf("trace\\.call: glBufferDataARB\\(GL_PIXEL_PACK_BUFFER(_EXT|_ARB)?, %u, NULL, GL_DYNAMIC_READ(_ARB)?\\)\n", pbo_size);
    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, pbo_ids[1]);
    test_log_printf("trace\\.call: glBindBufferARB\\(GL_PIXEL_UNPACK_BUFFER(_EXT|_ARB)?, %u\\)\n", pbo_ids[1]);
    glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, 512 * 512 * 4 + 4, NULL, GL_DYNAMIC_DRAW_ARB);
    test_log_printf("trace\\.call: glBufferDataARB\\(GL_PIXEL_UNPACK_BUFFER(_EXT|_ARB)?, %u, NULL, GL_DYNAMIC_DRAW(_ARB)?\\)\n", pbo_size);
}

static void pbo_teardown(void)
{
    if (!GLEW_ARB_pixel_buffer_object) return;

    glDeleteBuffersARB(2, pbo_ids);
    test_log_printf("trace\\.call: glDeleteBuffersARB\\(2, %p -> { %u, %u }\\)\n", pbo_ids, (unsigned int) pbo_ids[0], (unsigned int) pbo_ids[1]);
}

static void pbo_glTexImage2D(void)
{
    if (!check_support()) return;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, offset);
    test_log_printf("trace\\.call: glTexImage2D\\(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 4\\)\n");
}

static void pbo_glGetTexImage(void)
{
    if (!check_support()) return;

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, offset);
    test_log_printf("trace\\.call: glGetTexImage\\(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 4\\)\n");
}

static void pbo_glReadPixels(void)
{
    if (!check_support()) return;

    glReadPixels(0, 0, 300, 300, GL_RGBA, GL_UNSIGNED_BYTE, offset);
    test_log_printf("trace\\.call: glReadPixels\\(0, 0, 300, 300, GL_RGBA, GL_UNSIGNED_BYTE, 4\\)\n");
}

void pbo_suite_register(void)
{
    test_suite *ts = test_suite_new("pbo", TEST_FLAG_CONTEXT | TEST_FLAG_LOG, pbo_setup, pbo_teardown);
    test_suite_add_test(ts, "glTexImage2D", pbo_glTexImage2D);
    test_suite_add_test(ts, "glGetTexImage", pbo_glGetTexImage);
    test_suite_add_test(ts, "glReadPixels", pbo_glReadPixels);
}
