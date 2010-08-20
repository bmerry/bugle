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
    const GLushort short_indices[] = {2, 65535, 123};
    const GLuint int_indices[] = {3, 1000000, 1};
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

    if (GLEW_ARB_vertex_buffer_object)
    {
        glGenBuffersARB(1, &buffer);
        test_log_printf("trace\\.call: glGenBuffersARB\\(1, %p -> { %u }\\)\n",
                        &buffer, (unsigned int) buffer);
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffer);
        test_log_printf("trace\\.call: glBindBufferARB\\(GL_ELEMENT_ARRAY_BUFFER(_ARB)?, %u\\)\n",
                        (unsigned int) buffer);
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(byte_indices), byte_indices, GL_STATIC_DRAW_ARB);
        test_log_printf("trace\\.call: glBufferDataARB\\(GL_ELEMENT_ARRAY_BUFFER(_ARB)?, %u, %p -> { %u, %u, %u }, GL_STATIC_DRAW(_ARB)?\\)\n",
                        (unsigned int) sizeof(byte_indices), byte_indices,
                        byte_indices[0], byte_indices[1], byte_indices[2]);
        glDrawElements(GL_POINTS, 3, GL_UNSIGNED_BYTE, NULL);
        test_log_printf("trace\\.call: glDrawElements\\(GL_POINTS, 3, GL_UNSIGNED_BYTE, 0\\)\n");
    }
}

test_status draw_suite(void)
{
    draw_arrays();
    draw_elements();
    return TEST_RAN;
}
