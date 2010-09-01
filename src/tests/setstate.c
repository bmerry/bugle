/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2008, 2010  Bruce Merry
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

/* Sets a variety of OpenGL state, mainly to test the logger */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <stdlib.h>
#include <stdio.h>
#include "test.h"

/* Still TODO:
 * - glCompressedTexImage
 * - glLight
 * - glMaterial
 * - glClipPlane
 * - glGenBuffers, glDeleteBuffers
 * - glGenQueries, glDeleteQueries
 * - glConvolutionFilter1D
 * - glConvolutionFilter2D
 * - glSeparableFilter1D
 * - glPixelTransfer
 * - glPixelMap
 * - glColorTable
 * - glCopyColorTable
 * - glCopyColorSubTable
 * - glTexImage1D, glTexImage3D
 * - glTexSubImage1D, glTexSubImage2D, glTexSubImage3D
 * - glCopyTexImage1D, glCopyTexSubImage1D, glCopyTexImage2D, glCopyTexSubImage2D
 * - glVertexAttrib*
 * - glUniform*
 * - Anything >= GL 3.0
 */

static void set_enables(void)
{
    glEnable(GL_LIGHTING);             test_log_printf("trace\\.call: glEnable\\(GL_LIGHTING\\)\n");
    glEnable(GL_DEPTH_TEST);           test_log_printf("trace\\.call: glEnable\\(GL_DEPTH_TEST\\)\n");
    glEnable(GL_ALPHA_TEST);           test_log_printf("trace\\.call: glEnable\\(GL_ALPHA_TEST\\)\n");
    glEnable(GL_AUTO_NORMAL);          test_log_printf("trace\\.call: glEnable\\(GL_AUTO_NORMAL\\)\n");
    glEnable(GL_TEXTURE_1D);           test_log_printf("trace\\.call: glEnable\\(GL_TEXTURE_1D\\)\n");
    glEnable(GL_TEXTURE_2D);           test_log_printf("trace\\.call: glEnable\\(GL_TEXTURE_2D\\)\n");
    glEnable(GL_POLYGON_OFFSET_LINE);  test_log_printf("trace\\.call: glEnable\\(GL_POLYGON_OFFSET_LINE\\)\n");
    glEnable(GL_BLEND);                test_log_printf("trace\\.call: glEnable\\(GL_BLEND\\)\n");
}

static void set_enables_texture3D(void)
{
    if (GLEW_VERSION_1_2)
    {
        glEnable(GL_TEXTURE_3D);   test_log_printf("trace\\.call: glEnable\\(GL_TEXTURE_3D\\)\n");
    }
    else
        test_skipped("GL 1.2 required");
}

static void set_enables_cube_map(void)
{
    if (GLEW_VERSION_1_3)
    {
        glEnable(GL_TEXTURE_CUBE_MAP); test_log_printf("trace\\.call: glEnable\\(GL_TEXTURE_CUBE_MAP\\)\n");
    }
    else
        test_skipped("GL 1.3 required");
}

static void set_client_state(void)
{
    glEnableClientState(GL_VERTEX_ARRAY); test_log_printf("trace\\.call: glEnableClientState\\(GL_VERTEX_ARRAY\\)\n");
    glVertexPointer(3, GL_INT, 0, NULL);  test_log_printf("trace\\.call: glVertexPointer\\(3, GL_INT, 0, NULL\\)\n");
    glPixelStorei(GL_PACK_ALIGNMENT, 1);  test_log_printf("trace\\.call: glPixelStorei\\(GL_PACK_ALIGNMENT, 1\\)\n");
    glPixelStoref(GL_PACK_SWAP_BYTES, GL_FALSE); test_log_printf("trace\\.call: glPixelStoref\\(GL_PACK_SWAP_BYTES, GL_FALSE\\)\n");
}

static GLuint texture_id;

static void setup_texture_state(void)
{
    glGenTextures(1, &texture_id);
    test_log_printf("trace\\.call: glGenTextures\\(1, %p -> { %u }\\)\n", (void *) &texture_id, (unsigned int) texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    test_log_printf("trace\\.call: glBindTexture\\(GL_TEXTURE_2D, %u\\)\n", (unsigned int) texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("trace\\.call: glTexImage2D\\(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL\\)\n");
}

static void teardown_texture_state(void)
{
    glDeleteTextures(1, &texture_id);
    test_log_printf("trace\\.call: glDeleteTextures\\(1, %p -> { %u }\\)\n", (void *) &texture_id, (unsigned int) texture_id);
}

static void set_texture_state(void)
{
    GLint arg;

    setup_texture_state();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    test_log_printf("trace\\.call: glTexParameteri\\(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR\\)\n");
    arg = GL_LINEAR;
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &arg);
    test_log_printf("trace\\.call: glTexParameteriv\\(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, %p -> GL_LINEAR\\)\n", (void *) &arg);
    teardown_texture_state();
}

static void set_texture_state_mipmap(void)
{
    if (GLEW_VERSION_1_4)
    {
        setup_texture_state();
        glTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
        test_log_printf("trace\\.call: glTexParameterf\\(GL_TEXTURE_2D, GL_GENERATE_MIPMAP(_SGIS)?, GL_TRUE\\)\n");
        teardown_texture_state();
    }
    else
        test_skipped("GL 1.4 required");
}

static void set_material_state(void)
{
    GLfloat col[4] = {0.0, 0.25, 0.5, 1.0};
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, col);
    test_log_printf("trace\\.call: glMaterialfv\\(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, %p -> { 0, 0\\.25, 0\\.5, 1 }\\)\n",
                    (void *) col);
}

static void set_matrices(void)
{
    GLfloat m[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    glMultMatrixf(m);
    test_log_printf("trace\\.call: glMultMatrixf\\(%p -> { { %g, %g, %g, %g }, { %g, %g, %g, %g }, { %g, %g, %g, %g }, { %g, %g, %g, %g } }\\)\n",
                    m,
                    m[0], m[1], m[2], m[3],
                    m[4], m[5], m[6], m[7],
                    m[8], m[9], m[10], m[11],
                    m[12], m[13], m[14], m[15]);
}

static void set_map_buffer_range(void)
{
#ifdef GL_ARB_map_buffer_range
    if (GLEW_ARB_map_buffer_range)
    {
        GLuint id;
        glGenBuffers(1, &id);
        test_log_printf("trace\\.call: glGenBuffers\\(1, %p -> { %u }\\)\n",
                        &id, (unsigned int) id);
        glBindBuffer(GL_ARRAY_BUFFER, id);
        test_log_printf("trace\\.call: glBindBuffer\\(GL_ARRAY_BUFFER, %u\\)\n",
                        (unsigned int) id);
        glBufferData(GL_ARRAY_BUFFER, 128, NULL, GL_STATIC_DRAW);
        test_log_printf("trace\\.call: glBufferData\\(GL_ARRAY_BUFFER, 128, NULL, GL_STATIC_DRAW\\)\n");
        glMapBufferRange(GL_ARRAY_BUFFER, 16, 64, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
        test_log_printf("trace\\.call: glMapBufferRange\\(GL_ARRAY_BUFFER, 16, 64, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT\\)\n");
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 20, 4);
        test_log_printf("trace\\.call: glFlushMappedBufferRange\\(GL_ARRAY_BUFFER, 20, 4\\)\n");
        glDeleteBuffers(1, &id);
        test_log_printf("trace\\.call: glDeleteBuffers\\(1, %p -> { %u }\\)\n",
                        &id, (unsigned int) id);
    }
    else
#endif
    {
        test_skipped("GL_ARB_map_buffer_range required");
    }
}

void setstate_suite_register(void)
{
    test_suite *ts = test_suite_new("setstate", TEST_FLAG_LOG | TEST_FLAG_CONTEXT, NULL, NULL);
    test_suite_add_test(ts, "enables", set_enables);
    test_suite_add_test(ts, "enables_texture3D", set_enables_texture3D);
    test_suite_add_test(ts, "enables_cube_map", set_enables_cube_map);
    test_suite_add_test(ts, "client_state", set_client_state);
    test_suite_add_test(ts, "texture_state", set_texture_state);
    test_suite_add_test(ts, "texture_state_mipmap", set_texture_state_mipmap);
    test_suite_add_test(ts, "material_state", set_material_state);
    test_suite_add_test(ts, "matrices", set_matrices);
    test_suite_add_test(ts, "map_buffer_range", set_map_buffer_range);
}
