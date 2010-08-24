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

static void draw_arrays(void)
{
    glDrawArrays(GL_LINES, 5, 10);
    test_log_printf("trace\\.call: glDrawArrays\\(GL_LINES, 5, 10\\)\n");
}

static void draw_elements(void)
{
    const GLubyte byte_indices[] = {1, 255, 10};
    const GLushort short_indices[] = {0, 65535, 2, 5, 4, 3, 7, 8, 9};
    const GLuint int_indices[] = {3, 1000000, 1};
    /* Older versions of glext.h don't allow these to be const. */
    GLint first[2] = {5, 3};
    GLsizei count[2] = {3, 6};

    GLuint buffer;

    glDrawElements(GL_POINTS, 3, GL_UNSIGNED_BYTE, byte_indices);
    test_log_printf("trace\\.call: glDrawElements\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, %p -> { %u, %u, %u }\\)\n",
                    byte_indices, byte_indices[0], byte_indices[1], byte_indices[2]);
    glDrawElements(GL_POINTS, 3, GL_UNSIGNED_SHORT, short_indices);
    test_log_printf("trace\\.call: glDrawElements\\(GL_POINTS, 3, GL_UNSIGNED_SHORT, %p -> { %u, %u, %u }\\)\n",
                    short_indices, short_indices[0], short_indices[1], short_indices[2]);
    glDrawElements(GL_POINTS, 3, GL_UNSIGNED_INT, int_indices);
    test_log_printf("trace\\.call: glDrawElements\\(GL_POINTS, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }\\)\n",
                    int_indices, int_indices[0], int_indices[1], int_indices[2]);

    if (GLEW_VERSION_1_2)
    {
        glDrawRangeElements(GL_POINTS, 1, 1000000, 3, GL_UNSIGNED_INT, int_indices);
        test_log_printf("trace\\.call: glDrawRangeElements\\(GL_POINTS, 1, 1000000, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }\\)\n",
                        int_indices, int_indices[0], int_indices[1], int_indices[2]);
    }

    if (GLEW_VERSION_1_4)
    {
        const GLushort * pindices[2] = { &short_indices[6], &short_indices[0] };

        glMultiDrawArrays(GL_TRIANGLES, first, count, 2);
        test_log_printf("trace\\.call: glMultiDrawArrays\\(GL_TRIANGLES, %p -> { %d, %d }, %p -> { %d, %d }, 2\\)\n",
                        first, (int) first[0], (int) first[1],
                        count, (int) count[0], (int) count[1]);

        glMultiDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (const GLvoid **) pindices, 2);
        test_log_printf("trace\\.call: glMultiDrawElements\\(GL_TRIANGLES, %p -> { %d, %d }, GL_UNSIGNED_SHORT, %p -> { %p -> { %u, %u, %u }, %p -> { %u, %u, %u, %u, %u, %u } }, 2\\)\n",
                        count, (int) count[0], (int) count[1],
                        pindices,
                        pindices[0], pindices[0][0], pindices[0][1], pindices[0][2],
                        pindices[1], pindices[1][0], pindices[1][1], pindices[1][2], pindices[1][3], pindices[1][4], pindices[1][5]);
    }

    if (GLEW_VERSION_1_5)
    {
        const GLushort * pindices_vbo[2] =
        {
            (const GLushort *) NULL + 6,
            (const GLushort *) NULL + 0
        };

        glGenBuffers(1, &buffer);
        test_log_printf("trace\\.call: glGenBuffers\\(1, %p -> { %u }\\)\n",
                        &buffer, (unsigned int) buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
        test_log_printf("trace\\.call: glBindBuffer\\(GL_ELEMENT_ARRAY_BUFFER, %u\\)\n",
                        (unsigned int) buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(byte_indices), byte_indices, GL_STATIC_DRAW);
        test_log_printf("trace\\.call: glBufferData\\(GL_ELEMENT_ARRAY_BUFFER, %u, %p -> { %u, %u, %u }, GL_STATIC_DRAW\\)\n",
                        (unsigned int) sizeof(byte_indices), byte_indices,
                        byte_indices[0], byte_indices[1], byte_indices[2]);

        glDrawElements(GL_POINTS, 3, GL_UNSIGNED_BYTE, NULL);
        test_log_printf("trace\\.call: glDrawElements\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, 0\\)\n");

        glDrawRangeElements(GL_POINTS, 0, 255, 3, GL_UNSIGNED_BYTE, NULL);
        test_log_printf("trace\\.call: glDrawRangeElements\\(GL_POINTS, 0, 255, 3, GL_UNSIGNED_BYTE, 0\\)\n");

        glMultiDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (const GLvoid **) pindices_vbo, 2);
        test_log_printf("trace\\.call: glMultiDrawElements\\(GL_TRIANGLES, %p -> { %d, %d }, GL_UNSIGNED_SHORT, %p -> { 12, 0 }, 2\\)\n",
                        count, (int) count[0], (int) count[1],
                        pindices_vbo);

        if (GLEW_ARB_draw_elements_base_vertex)
        {
            const GLint base_vertices[2] = {0, 50};

            glDrawElementsBaseVertex(GL_POINTS, 3, GL_UNSIGNED_BYTE, NULL, 1000);
            test_log_printf("trace\\.call: glDrawElementsBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, 0, 1000\\)\n");

            glDrawElementsInstancedBaseVertex(GL_POINTS, 3, GL_UNSIGNED_BYTE, NULL, 17, 1000);
            test_log_printf("trace\\.call: glDrawElementsInstancedBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, 0, 17, 1000\\)\n");

            glMultiDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (const GLvoid **) pindices_vbo, 2, base_vertices);
            test_log_printf("trace\\.call: glMultiDrawElementsBaseVertex\\(GL_TRIANGLES, %p -> { %d, %d }, GL_UNSIGNED_SHORT, %p -> { 12, 0 }, 2, %p -> { %d, %d }\\)\n",
                            count, (int) count[0], (int) count[1],
                            pindices_vbo,
                            base_vertices, (int) base_vertices[0], (int) base_vertices[1]);
        }

        glDeleteBuffers(1, &buffer);
        test_log_printf("trace\\.call: glDeleteBuffers\\(1, %p -> { %u }\\)\n",
                        &buffer, (unsigned int) buffer);
    }

#ifdef GL_ARB_draw_elements_base_vertex
    if (GLEW_ARB_draw_elements_base_vertex)
    {
        const GLushort * pindices[2] = { &short_indices[6], &short_indices[0] };
        const GLint base_vertices[2] = {0, 50};

        glDrawElementsBaseVertex(GL_POINTS, 3, GL_UNSIGNED_BYTE, byte_indices, 0);
        test_log_printf("trace\\.call: glDrawElementsBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, %p -> { %u, %u, %u }, 0\\)\n",
                        byte_indices, byte_indices[0], byte_indices[1], byte_indices[2]);
        glDrawElementsBaseVertex(GL_POINTS, 3, GL_UNSIGNED_SHORT, short_indices, 1);
        test_log_printf("trace\\.call: glDrawElementsBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_SHORT, %p -> { %u, %u, %u }, 1\\)\n",
                        short_indices, short_indices[0], short_indices[1], short_indices[2]);
        glDrawElementsBaseVertex(GL_POINTS, 3, GL_UNSIGNED_INT, int_indices, 2);
        test_log_printf("trace\\.call: glDrawElementsBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }, 2\\)\n",
                        int_indices, int_indices[0], int_indices[1], int_indices[2]);

        glDrawElementsInstancedBaseVertex(GL_POINTS, 3, GL_UNSIGNED_INT, int_indices, 20, 2);
        test_log_printf("trace\\.call: glDrawElementsInstancedBaseVertex\\(GL_POINTS, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }, 20, 2\\)\n",
                        int_indices, int_indices[0], int_indices[1], int_indices[2]);

        glDrawRangeElementsBaseVertex(GL_POINTS, 0, 255, 3, GL_UNSIGNED_INT, int_indices, 2);
        test_log_printf("trace\\.call: glDrawRangeElementsBaseVertex\\(GL_POINTS, 0, 255, 3, GL_UNSIGNED_INT, %p -> { %u, %u, %u }, 2\\)\n",
                        int_indices, int_indices[0], int_indices[1], int_indices[2]);

        glMultiDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (const GLvoid **) pindices, 2, base_vertices);
        test_log_printf("trace\\.call: glMultiDrawElementsBaseVertex\\(GL_TRIANGLES, %p -> { %d, %d }, GL_UNSIGNED_SHORT, %p -> { %p -> { %u, %u, %u }, %p -> { %u, %u, %u, %u, %u, %u } }, 2, %p -> { %d, %d }\\)\n",
                        count, (int) count[0], (int) count[1],
                        pindices,
                        pindices[0], pindices[0][0], pindices[0][1], pindices[0][2],
                        pindices[1], pindices[1][0], pindices[1][1], pindices[1][2], pindices[1][3], pindices[1][4], pindices[1][5],
                        base_vertices, (int) base_vertices[0], (int) base_vertices[1]);
    }
#endif
}

test_status draw_suite(void)
{
    draw_arrays();
    draw_elements();
    return TEST_RAN;
}
