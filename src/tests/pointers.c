/* Pass a variety of illegal pointers to OpenGL */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include "test.h"

static void invalid_direct(void)
{
    GLfloat v[3] = {0.0f, 0.0f, 0.0f};
    GLuint i[4] = {0, 0, 0, 0};
    glVertexPointer(3, GL_FLOAT, 0, v);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
    test_log_printf("checks\\.error: illegal index array caught in glDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_INT, i); /* legal */
#ifdef GL_EXT_draw_range_elements
    if (GLEW_EXT_draw_range_elements)
    {
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 500, GL_UNSIGNED_INT, NULL);
        test_log_printf("checks\\.error: illegal index array caught in glDrawRangeElements \\(unreadable memory\\); call will be ignored\\.\n");
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, i); /* legal */
    }
#endif
    glDisableClientState(GL_VERTEX_ARRAY);

    glTexCoordPointer(3, GL_FLOAT, 0, v);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
    test_log_printf("checks\\.error: illegal index array caught in glDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
#ifdef GL_ARB_multitexture
    if (GLEW_ARB_multitexture)
    {
        glClientActiveTextureARB(GL_TEXTURE1_ARB);
        glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
        test_log_printf("checks\\.error: illegal index array caught in glDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
        glClientActiveTextureARB(GL_TEXTURE0_ARB);
    }
#endif
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void invalid_direct_vbo(void)
{
#ifdef GL_ARB_vertex_buffer_object
    if (GLEW_ARB_vertex_buffer_object)
    {
        GLfloat v[3] = {0.0f, 0.0f, 0.0f};
        GLuint indices[4] = {0, 0, 0, 0};
        GLuint id;

        glVertexPointer(3, GL_FLOAT, 0, v);
        glEnableClientState(GL_VERTEX_ARRAY);

        glGenBuffersARB(1, &id);
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, id);
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(indices),
                        indices, GL_STATIC_DRAW_ARB);
        glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
        test_log_printf("checks\\.error: illegal index array caught in glDrawElements \\(VBO overrun\\); call will be ignored\\.\n");
        glDrawElements(GL_POINTS, 4, GL_UNSIGNED_INT, NULL); /* legal */
#ifdef GL_EXT_draw_range_elements
        if (GLEW_EXT_draw_range_elements)
        {
            glDrawRangeElementsEXT(GL_POINTS, 0, 0, 500, GL_UNSIGNED_INT, NULL);
            test_log_printf("checks\\.error: illegal index array caught in glDrawRangeElements \\(VBO overrun\\); call will be ignored\\.\n");
            glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, NULL); /* legal */
        }
#endif
#ifdef GL_EXT_multi_draw_arrays
        if (GLEW_EXT_multi_draw_arrays)
        {
            GLsizei count = 500;
            const GLvoid *indices = NULL;
            glMultiDrawElementsEXT(GL_POINTS, &count, GL_UNSIGNED_INT, &indices, 1);
            test_log_printf("checks\\.error: illegal index array caught in glMultiDrawElements \\(VBO overrun\\); call will be ignored\\.\n");

            count = 4;
            glMultiDrawElementsEXT(GL_POINTS, &count, GL_UNSIGNED_INT, &indices, 1);
            /* legal */
        }

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        glDeleteBuffersARB(1, &id);
        glDisableClientState(GL_VERTEX_ARRAY);
#endif
    }
#endif /* GL_ARB_vertex_buffer_object */
}

static void invalid_indirect(void)
{
    GLuint i[4] = {0, 0, 0, 0};

    glVertexPointer(3, GL_FLOAT, 0, NULL);
    glEnableClientState(GL_VERTEX_ARRAY);

    glDrawArrays(GL_POINTS, 0, 500);
    test_log_printf("checks\\.error: illegal vertex array caught in glDrawArrays \\(unreadable memory\\); call will be ignored\\.\n");
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_INT, i);
    test_log_printf("checks\\.error: illegal vertex array caught in glDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
#ifdef GL_EXT_draw_range_elements
    if (GLEW_EXT_draw_range_elements)
    {
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, i);
        test_log_printf("checks\\.error: illegal vertex array caught in glDrawRangeElements \\(unreadable memory\\); call will be ignored\\.\n");
    }
#endif
#ifdef GL_EXT_multi_draw_arrays
    if (GLEW_EXT_multi_draw_arrays)
    {
        GLint first = 0;
        GLsizei count = 1;
        const GLvoid *indices = &i;
        glMultiDrawArraysEXT(GL_POINTS, &first, &count, 1);
        test_log_printf("checks\\.error: illegal vertex array caught in glMultiDrawArrays \\(unreadable memory\\); call will be ignored\\.\n");
        glMultiDrawElementsEXT(GL_POINTS, &count, GL_UNSIGNED_INT, &indices, 1);
        test_log_printf("checks\\.error: illegal vertex array caught in glMultiDrawElements \\(unreadable memory\\); call will be ignored\\.\n");
    }
#endif
    glDisableClientState(GL_VERTEX_ARRAY);

#ifdef GL_ARB_vertex_program
    if (GLEW_ARB_vertex_program)
    {
        glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(3);
        glDrawArrays(GL_POINTS, 0, 500);
        test_log_printf("checks\\.error: illegal generic attribute array 3 caught in glDrawArrays \\(unreadable memory\\); call will be ignored\\.\n");
        glDisableVertexAttribArray(3);
    }
#endif
}

static void invalid_range(void)
{
#ifdef GL_EXT_draw_range_elements
    GLfloat v[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    GLuint i[4] = {0, 1, 0, 1};
    glVertexPointer(3, GL_FLOAT, 0, v);
    glEnableClientState(GL_VERTEX_ARRAY);

    if (GLEW_EXT_draw_range_elements)
    {
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, i);
        test_log_printf("checks\\.error: glDrawRangeElements indices fall outside range; call will be ignored\\.\n");
        glDrawRangeElementsEXT(GL_POINTS, 1, 1, 4, GL_UNSIGNED_INT, i);
        test_log_printf("checks\\.error: glDrawRangeElements indices fall outside range; call will be ignored\\.\n");
        glDrawRangeElementsEXT(GL_POINTS, 0, 0xffffff, 4, GL_UNSIGNED_INT, i);
        test_log_printf("checks\\.error: illegal vertex array caught in glDrawRangeElements \\(unreadable memory\\); call will be ignored\\.\n");
    }
#endif
}

test_status pointers_suite(void)
{
    invalid_direct();
    invalid_direct_vbo();
    invalid_indirect();
    invalid_range();
    return TEST_RAN;
}
