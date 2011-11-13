/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2010  Bruce Merry
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

/* Pass a variety of illegal pointers to OpenGL */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <bugle/bool.h>
#include "test.h"

static GLfloat vertices[3] = {0.0f, 0.0f, 0.0f};
static GLushort indices[4] = {0, 0, 0, 0};

static bugle_bool check_support(void)
{
#ifdef BUGLE_PLATFORM_POSIX
    return BUGLE_TRUE;
#else
    test_skipped("not a POSIX platform");
    return BUGLE_FALSE;
#endif
}

static void invalid_direct(void)
{
    if (!check_support()) return;

    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawElements(GL_POINTS, 500, GL_UNSIGNED_SHORT, NULL);
    test_log_printf("checks\\.error: illegal index array caught in glDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_SHORT, indices); /* legal */
    glDisableClientState(GL_VERTEX_ARRAY);

    glTexCoordPointer(3, GL_FLOAT, 0, vertices);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDrawElements(GL_POINTS, 500, GL_UNSIGNED_SHORT, NULL);
    test_log_printf("checks\\.error: illegal index array caught in glDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void invalid_direct_draw_range_elements(void)
{
    if (!check_support()) return;

    if (GLEW_VERSION_1_2)
    {
        glVertexPointer(3, GL_FLOAT, 0, vertices);
        glEnableClientState(GL_VERTEX_ARRAY);
        glDrawRangeElements(GL_POINTS, 0, 0, 500, GL_UNSIGNED_SHORT, NULL);
        test_log_printf("checks\\.error: illegal index array caught in glDrawRangeElements \\(unreadable memory\\); call will be ignored\\.\n");
        glDrawRangeElements(GL_POINTS, 0, 0, 4, GL_UNSIGNED_SHORT, indices); /* legal */
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    else
        test_skipped("GL 1.2 required");
}

static void invalid_direct_multitexture(void)
{
    if (!check_support()) return;

    if (GLEW_VERSION_1_3)
    {
        glClientActiveTexture(GL_TEXTURE1);
        glTexCoordPointer(3, GL_FLOAT, 0, vertices);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glDrawElements(GL_POINTS, 500, GL_UNSIGNED_SHORT, NULL);
        test_log_printf("checks\\.error: illegal index array caught in glDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glClientActiveTextureARB(GL_TEXTURE0);
    }
    else
        test_skipped("GL 1.3 required");
}

static void invalid_direct_vbo(void)
{
    if (GLEW_VERSION_1_5)
    {
        GLuint id;
        GLsizei count = 500;
        const GLvoid *pindices = NULL;

        glVertexPointer(3, GL_FLOAT, 0, vertices);
        glEnableClientState(GL_VERTEX_ARRAY);

        glGenBuffers(1, &id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices),
                     indices, GL_STATIC_DRAW);
        glDrawElements(GL_POINTS, 500, GL_UNSIGNED_SHORT, NULL);
        test_log_printf("checks\\.error: illegal index array caught in glDrawElements \\(VBO overrun\\); call will be ignored\\.\n");
        glDrawElements(GL_POINTS, 4, GL_UNSIGNED_SHORT, NULL); /* legal */

        glDrawRangeElements(GL_POINTS, 0, 0, 500, GL_UNSIGNED_SHORT, NULL);
        test_log_printf("checks\\.error: illegal index array caught in glDrawRangeElements \\(VBO overrun\\); call will be ignored\\.\n");
        glDrawRangeElements(GL_POINTS, 0, 0, 4, GL_UNSIGNED_SHORT, NULL); /* legal */

        glMultiDrawElementsEXT(GL_POINTS, &count, GL_UNSIGNED_SHORT, &pindices, 1);
        test_log_printf("checks\\.error: illegal index array caught in glMultiDrawElementsEXT \\(VBO overrun\\); call will be ignored\\.\n");

        count = 4;
        glMultiDrawElementsEXT(GL_POINTS, &count, GL_UNSIGNED_SHORT, &pindices, 1);
        /* legal */

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &id);
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    else
        test_skipped("GL 1.5 required");
}

static void invalid_indirect(void)
{
    if (!check_support()) return;

    glVertexPointer(3, GL_FLOAT, 0, NULL);
    glEnableClientState(GL_VERTEX_ARRAY);

    glDrawArrays(GL_POINTS, 0, 500);
    test_log_printf("checks\\.error: illegal vertex array caught in glDrawArrays \\(unreadable memory\\); call will be ignored\\.\n");
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_SHORT, indices);
    test_log_printf("checks\\.error: illegal vertex array caught in glDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
    glDisableClientState(GL_VERTEX_ARRAY);
}

static void invalid_indirect_attrib_array(void)
{
    if (!check_support()) return;

    if (GLEW_VERSION_2_0)
    {
        glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(3);
        glDrawArrays(GL_POINTS, 0, 500);
        test_log_printf("checks\\.error: illegal generic attribute array 3 caught in glDrawArrays \\(unreadable memory\\); call will be ignored\\.\n");
        glDisableVertexAttribArray(3);
    }
    else
        test_skipped("GL 2.0 required");
}

static void invalid_indirect_draw_range_elements(void)
{
    if (!check_support()) return;

    if (GLEW_VERSION_1_2)
    {
        glVertexPointer(3, GL_FLOAT, 0, NULL);
        glEnableClientState(GL_VERTEX_ARRAY);

        glDrawRangeElements(GL_POINTS, 0, 0, 4, GL_UNSIGNED_SHORT, indices);
        test_log_printf("checks\\.error: illegal vertex array caught in glDrawRangeElements \\(unreadable memory\\); call will be ignored\\.\n");
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    else
        test_skipped("GL 1.2 required");
}

static void invalid_indirect_multi_draw(void)
{
    if (!check_support()) return;

    if (GLEW_VERSION_1_4)
    {
        GLint first = 0;
        GLsizei count = 1;
        const GLvoid *pindices = &indices;

        glVertexPointer(3, GL_FLOAT, 0, NULL);
        glEnableClientState(GL_VERTEX_ARRAY);
        glMultiDrawArrays(GL_POINTS, &first, &count, 1);
        test_log_printf("checks\\.error: illegal vertex array caught in glMultiDrawArrays \\(unreadable memory\\); call will be ignored\\.\n");
        glMultiDrawElements(GL_POINTS, &count, GL_UNSIGNED_SHORT, &pindices, 1);
        test_log_printf("checks\\.error: illegal vertex array caught in glMultiDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    else
        test_skipped("GL 1.4 required");
}

static void invalid_range(void)
{
    if (GLEW_VERSION_1_2)
    {
        GLfloat range_vertices[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        GLushort range_indices[4] = {0, 1, 0, 1};
        glVertexPointer(3, GL_FLOAT, 0, range_vertices);
        glEnableClientState(GL_VERTEX_ARRAY);

        glDrawRangeElements(GL_POINTS, 0, 0, 4, GL_UNSIGNED_SHORT, range_indices);
        test_log_printf("checks\\.error: glDrawRangeElements indices fall outside range; call will be ignored\\.\n");
        glDrawRangeElements(GL_POINTS, 1, 1, 4, GL_UNSIGNED_SHORT, range_indices);
        test_log_printf("checks\\.error: glDrawRangeElements indices fall outside range; call will be ignored\\.\n");
        glDrawRangeElements(GL_POINTS, 0, 0xffffff, 4, GL_UNSIGNED_SHORT, range_indices);
        test_log_printf("checks\\.error: illegal vertex array caught in glDrawRangeElements \\(unreadable memory\\); call will be ignored\\.\n");

        glDisableClientState(GL_VERTEX_ARRAY);
    }
    else
        test_skipped("GL 1.2 required");
}

void pointers_suite_register(void)
{
    test_suite *ts = test_suite_new("pointers", TEST_FLAG_CONTEXT | TEST_FLAG_LOG, NULL, NULL);
    test_suite_add_test(ts, "direct", invalid_direct);
    test_suite_add_test(ts, "direct_draw_range_elements", invalid_direct_draw_range_elements);
    test_suite_add_test(ts, "direct_multitexture", invalid_direct_multitexture);
    test_suite_add_test(ts, "direct_vbo", invalid_direct_vbo);
    test_suite_add_test(ts, "indirect", invalid_indirect);
    test_suite_add_test(ts, "indirect_attrib_array", invalid_indirect_attrib_array);
    test_suite_add_test(ts, "indirect_draw_range_elements", invalid_indirect_draw_range_elements);
    test_suite_add_test(ts, "indirect_multi_draw", invalid_indirect_multi_draw);
    test_suite_add_test(ts, "indirect_range", invalid_range);
}
