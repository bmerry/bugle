/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2010  Bruce Merry
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

/* Makes some draw calls, to test the logger */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <stdlib.h>
#include <stdio.h>
#include "test.h"

static GLubyte byte_indices[] = {1, 255, 10};
static GLushort short_indices[] = {0, 65535, 2, 5, 4, 3, 7, 8, 9};
static GLuint int_indices[] = {3, 1000000, 1};
#ifdef GL_ARB_draw_elements_base_vertex
static GLint base_vertices[2] = {0, 50};
#endif
static GLint first[2] = {5, 3};
static GLsizei count[2] = {3, 6};
static const GLushort * ptr_short_indices[2];
static const GLushort * ptr_short_indices_vbo[2] =
{
    (const GLushort *) NULL + 6,
    (const GLushort *) NULL + 0
};
static GLuint index_buffer = 0;

static void draw_arrays(void)
{
    glDrawArrays(GL_LINES, 5, 10);
    test_log_printf("trace\\.call: glDrawArrays\\(GL_LINES, 5, 10\\)\n");
}

static void draw_elements_client(void)
{
    glDrawElements(GL_POINTS, 3, GL_UNSIGNED_BYTE, byte_indices);
    test_log_printf("trace\\.call: glDrawElements\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, %p -> { %u, %u, %u }\\)\n",
                    byte_indices, byte_indices[0], byte_indices[1], byte_indices[2]);
    glDrawElements(GL_POINTS, 3, GL_UNSIGNED_SHORT, short_indices);
    test_log_printf("trace\\.call: glDrawElements\\(GL_POINTS, 3, GL_UNSIGNED_SHORT, %p -> { %u, %u, %u }\\)\n",
                    short_indices, short_indices[0], short_indices[1], short_indices[2]);
    glDrawElements(GL_POINTS, 3, GL_UNSIGNED_INT, int_indices);
    test_log_printf("trace\\.call: glDrawElements\\(GL_POINTS, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }\\)\n",
                    int_indices, int_indices[0], int_indices[1], int_indices[2]);
}

static void draw_elements_vbo(void)
{
    if (GLEW_VERSION_1_5)
    {
        glDrawElements(GL_POINTS, 3, GL_UNSIGNED_BYTE, NULL);
        test_log_printf("trace\\.call: glDrawElements\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, 0\\)\n");
    }
    else
        test_skipped("GL 1.5 required");
}

static void draw_range_elements_client(void)
{
    if (GLEW_VERSION_1_2)
    {
        glDrawRangeElements(GL_POINTS, 1, 1000000, 3, GL_UNSIGNED_INT, int_indices);
        test_log_printf("trace\\.call: glDrawRangeElements\\(GL_POINTS, 1, 1000000, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }\\)\n",
                        int_indices, int_indices[0], int_indices[1], int_indices[2]);
    }
    else
        test_skipped("GL 1.2 required");
}

static void draw_range_elements_vbo(void)
{
    if (GLEW_VERSION_1_5)
    {
        glDrawRangeElements(GL_POINTS, 0, 255, 3, GL_UNSIGNED_BYTE, NULL);
        test_log_printf("trace\\.call: glDrawRangeElements\\(GL_POINTS, 0, 255, 3, GL_UNSIGNED_BYTE, 0\\)\n");
    }
    else
        test_skipped("GL 1.5 required");
}

static void multi_draw_arrays(void)
{
    if (GLEW_VERSION_1_4)
    {
        glMultiDrawArrays(GL_TRIANGLES, first, count, 2);
        test_log_printf("trace\\.call: glMultiDrawArrays\\(GL_TRIANGLES, %p -> { %d, %d }, %p -> { %d, %d }, 2\\)\n",
                        first, (int) first[0], (int) first[1],
                        count, (int) count[0], (int) count[1]);
    }
    else
        test_skipped("GL 1.4 required");
}

static void multi_draw_elements_client(void)
{
    if (GLEW_VERSION_1_4)
    {
        glMultiDrawArrays(GL_TRIANGLES, first, count, 2);
        test_log_printf("trace\\.call: glMultiDrawArrays\\(GL_TRIANGLES, %p -> { %d, %d }, %p -> { %d, %d }, 2\\)\n",
                        first, (int) first[0], (int) first[1],
                        count, (int) count[0], (int) count[1]);

        glMultiDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (const GLvoid **) ptr_short_indices, 2);
        test_log_printf("trace\\.call: glMultiDrawElements\\(GL_TRIANGLES, %p -> { %d, %d }, GL_UNSIGNED_SHORT, %p -> { %p -> { %u, %u, %u }, %p -> { %u, %u, %u, %u, %u, %u } }, 2\\)\n",
                        count, (int) count[0], (int) count[1],
                        ptr_short_indices,
                        ptr_short_indices[0], ptr_short_indices[0][0], ptr_short_indices[0][1], ptr_short_indices[0][2],
                        ptr_short_indices[1], ptr_short_indices[1][0], ptr_short_indices[1][1], ptr_short_indices[1][2], ptr_short_indices[1][3], ptr_short_indices[1][4], ptr_short_indices[1][5]);
    }
    else
        test_skipped("GL 1.4 required");
}

static void multi_draw_elements_vbo(void)
{
    if (GLEW_VERSION_1_5)
    {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(short_indices), short_indices, GL_STATIC_DRAW);
        test_log_printf("trace\\.call: glBufferData\\(GL_ELEMENT_ARRAY_BUFFER, %u, %p -> {[0-9 ,]+}, GL_STATIC_DRAW\\)\n",
                        (unsigned int) sizeof(short_indices), short_indices);
        glMultiDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (const GLvoid **) ptr_short_indices_vbo, 2);
        test_log_printf("trace\\.call: glMultiDrawElements\\(GL_TRIANGLES, %p -> { %d, %d }, GL_UNSIGNED_SHORT, %p -> { 12, 0 }, 2\\)\n",
                        count, (int) count[0], (int) count[1],
                        ptr_short_indices_vbo);
    }
    else
        test_skipped("GL 1.5 required");
}

static void draw_elements_base_vertex_client(void)
{
#ifdef GL_ARB_draw_elements_base_vertex
    if (GLEW_ARB_draw_elements_base_vertex)
    {
        glDrawElementsBaseVertex(GL_POINTS, 3, GL_UNSIGNED_BYTE, byte_indices, 0);
        test_log_printf("trace\\.call: glDrawElementsBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, %p -> { %u, %u, %u }, 0\\)\n",
                        byte_indices, byte_indices[0], byte_indices[1], byte_indices[2]);
        glDrawElementsBaseVertex(GL_POINTS, 3, GL_UNSIGNED_SHORT, short_indices, 1);
        test_log_printf("trace\\.call: glDrawElementsBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_SHORT, %p -> { %u, %u, %u }, 1\\)\n",
                        short_indices, short_indices[0], short_indices[1], short_indices[2]);
        glDrawElementsBaseVertex(GL_POINTS, 3, GL_UNSIGNED_INT, int_indices, 2);
        test_log_printf("trace\\.call: glDrawElementsBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }, 2\\)\n",
                        int_indices, int_indices[0], int_indices[1], int_indices[2]);
    }
    else
#endif
    {
        test_skipped("ARB_draw_elements_base_vertex required");
    }
}

static void draw_elements_base_vertex_vbo(void)
{
#ifdef GL_ARB_draw_elements_base_vertex
    if (GLEW_ARB_draw_elements_base_vertex && GLEW_VERSION_1_5)
    {
        glDrawElementsBaseVertex(GL_POINTS, 3, GL_UNSIGNED_BYTE, NULL, 1000);
        test_log_printf("trace\\.call: glDrawElementsBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, 0, 1000\\)\n");
    }
    else
#endif
    {
        test_skipped("ARB_draw_elements_base_vertex and GL 1.5 required");
    }
}

static void draw_range_elements_base_vertex_client(void)
{
#ifdef GL_ARB_draw_elements_base_vertex
    if (GLEW_ARB_draw_elements_base_vertex)
    {
        glDrawRangeElementsBaseVertex(GL_POINTS, 0, 255, 3, GL_UNSIGNED_INT, int_indices, 2);
        test_log_printf("trace\\.call: glDrawRangeElementsBaseVertex\\(GL_POINTS, 0, 255, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }, 2\\)\n",
                        int_indices, int_indices[0], int_indices[1], int_indices[2]);
    }
    else
#endif
    {
        test_skipped("ARB_draw_elements_base_vertex required");
    }
}

static void draw_range_elements_base_vertex_vbo(void)
{
#ifdef GL_ARB_draw_elements_base_vertex
    if (GLEW_ARB_draw_elements_base_vertex && GLEW_VERSION_1_5)
    {
        glDrawElementsBaseVertex(GL_POINTS, 3, GL_UNSIGNED_BYTE, NULL, 1000);
        test_log_printf("trace\\.call: glDrawElementsBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, 0, 1000\\)\n");
    }
    else
#endif
    {
        test_skipped("ARB_draw_elements_base_vertex and GL 1.5 required");
    }
}

static void draw_elements_instanced_base_vertex_client(void)
{
#ifdef GL_ARB_draw_elements_base_vertex
    if (GLEW_ARB_draw_elements_base_vertex)
    {
        glDrawElementsInstancedBaseVertex(GL_POINTS, 3, GL_UNSIGNED_INT, int_indices, 20, 2);
        test_log_printf("trace\\.call: glDrawElementsInstancedBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }, 20, 2\\)\n",
                        int_indices, int_indices[0], int_indices[1], int_indices[2]);
    }
    else
#endif
    {
        test_skipped("ARB_draw_elements_base_vertex required");
    }
}

static void draw_elements_instanced_base_vertex_vbo(void)
{
#ifdef GL_ARB_draw_elements_base_vertex
    if (GLEW_ARB_draw_elements_base_vertex && GLEW_VERSION_1_5)
    {
        glDrawElementsInstancedBaseVertex(GL_POINTS, 3, GL_UNSIGNED_BYTE, NULL, 17, 1000);
        test_log_printf("trace\\.call: glDrawElementsInstancedBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, 0, 17, 1000\\)\n");
    }
    else
#endif
    {
        test_skipped("ARB_draw_elements_base_vertex and GL 1.5 required");
    }
}

static void multi_draw_elements_base_vertex_client(void)
{
#ifdef GL_ARB_draw_elements_base_vertex
    if (GLEW_ARB_draw_elements_base_vertex)
    {
        glMultiDrawElementsBaseVertex(GL_LINES, count, GL_UNSIGNED_SHORT, (const GLvoid **) ptr_short_indices, 2, base_vertices);
        test_log_printf("trace\\.call: glMultiDrawElementsBaseVertex\\(GL_LINES, %p -> { %d, %d }, GL_UNSIGNED_SHORT, %p -> { %p -> { %u, %u, %u }, %p -> { %u, %u, %u, %u, %u, %u } }, 2, %p -> { %d, %d }\\)\n",
                        count, (int) count[0], (int) count[1],
                        ptr_short_indices,
                        ptr_short_indices[0], ptr_short_indices[0][0], ptr_short_indices[0][1], ptr_short_indices[0][2],
                        ptr_short_indices[1], ptr_short_indices[1][0], ptr_short_indices[1][1], ptr_short_indices[1][2], ptr_short_indices[1][3], ptr_short_indices[1][4], ptr_short_indices[1][5],
                        base_vertices, (int) base_vertices[0], (int) base_vertices[1]);
    }
    else
#endif
    {
        test_skipped("ARB_draw_elements_base_vertex required");
    }
}

static void multi_draw_elements_base_vertex_vbo(void)
{
#ifdef GL_ARB_draw_elements_base_vertex
    if (GLEW_ARB_draw_elements_base_vertex && GLEW_VERSION_1_5)
    {
            glMultiDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (const GLvoid **) ptr_short_indices_vbo, 2, base_vertices);
            test_log_printf("trace\\.call: glMultiDrawElementsBaseVertex\\(GL_TRIANGLES, %p -> { %d, %d }, GL_UNSIGNED_SHORT, %p -> { 12, 0 }, 2, %p -> { %d, %d }\\)\n",
                            count, (int) count[0], (int) count[1],
                            ptr_short_indices_vbo,
                            base_vertices, (int) base_vertices[0], (int) base_vertices[1]);
    }
    else
#endif
    {
        test_skipped("ARB_draw_elements_base_vertex and GL 1.5 required");
    }
}

static void setup_buffer(void)
{
    if (GLEW_VERSION_1_5)
    {
        glGenBuffers(1, &index_buffer);
        test_log_printf("trace\\.call: glGenBuffers\\(1, %p -> { %u }\\)\n",
                        &index_buffer, (unsigned int) index_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
        test_log_printf("trace\\.call: glBindBuffer\\(GL_ELEMENT_ARRAY_BUFFER, %u\\)\n",
                        (unsigned int) index_buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(byte_indices), byte_indices, GL_STATIC_DRAW);
        test_log_printf("trace\\.call: glBufferData\\(GL_ELEMENT_ARRAY_BUFFER, %u, %p -> { %u, %u, %u }, GL_STATIC_DRAW\\)\n",
                        (unsigned int) sizeof(byte_indices), byte_indices,
                        byte_indices[0], byte_indices[1], byte_indices[2]);
    }
}

static void teardown_buffer(void)
{
    if (GLEW_VERSION_1_5)
    {
        glDeleteBuffers(1, &index_buffer);
        test_log_printf("trace\\.call: glDeleteBuffers\\(1, %p -> { %u }\\)\n",
                        &index_buffer, (unsigned int) index_buffer);
    }
}

void draw_suite_register(void)
{
    test_suite *ts;

    ptr_short_indices[0] = &short_indices[6];
    ptr_short_indices[1] = &short_indices[0];

    ts = test_suite_new("draw_client", TEST_FLAG_CONTEXT | TEST_FLAG_LOG, NULL, NULL);
    test_suite_add_test(ts, "glDrawArrays", draw_arrays);
    test_suite_add_test(ts, "glDrawElements", draw_elements_client);
    test_suite_add_test(ts, "glDrawRangeElements", draw_range_elements_client);
    test_suite_add_test(ts, "glMultiDrawArrays", multi_draw_arrays);
    test_suite_add_test(ts, "glMultiDrawElements", multi_draw_elements_client);
    test_suite_add_test(ts, "glDrawElementsBaseVertex", draw_elements_base_vertex_client);
    test_suite_add_test(ts, "glDrawRangeElementsBaseVertex", draw_range_elements_base_vertex_client);
    test_suite_add_test(ts, "glDrawElementsInstancedBaseVertex", draw_elements_instanced_base_vertex_client);
    test_suite_add_test(ts, "glMultiDrawElementsBaseVertex", multi_draw_elements_base_vertex_client);

    ts = test_suite_new("draw_vbo", TEST_FLAG_CONTEXT | TEST_FLAG_LOG, setup_buffer, teardown_buffer);
    test_suite_add_test(ts, "glDrawElements", draw_elements_vbo);
    test_suite_add_test(ts, "glDrawRangeElements", draw_range_elements_vbo);
    test_suite_add_test(ts, "glMultiDrawElements", multi_draw_elements_vbo);
    test_suite_add_test(ts, "glDrawElementsBaseVertex", draw_elements_base_vertex_vbo);
    test_suite_add_test(ts, "glDrawRangeElementsBaseVertex", draw_range_elements_base_vertex_vbo);
    test_suite_add_test(ts, "glDrawElementsInstancedBaseVertex", draw_elements_instanced_base_vertex_vbo);
    test_suite_add_test(ts, "glMultiDrawElementsBaseVertex", multi_draw_elements_base_vertex_vbo);
}
