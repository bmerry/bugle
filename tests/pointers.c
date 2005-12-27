/* Pass a variety of illegal pointers to OpenGL */
#ifdef NDEBUG /* make sure that assert is defined */
# undef NDEBUG
#endif

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _POSIX_SOURCE
#include "tests/loader.h"
#include <GL/glut.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static FILE *ref;

static void invalid_direct(void)
{
    GLfloat v[3] = {0.0f, 0.0f, 0.0f};
    GLuint i[4] = {0, 0, 0, 0};
    glVertexPointer(3, GL_FLOAT, 0, v);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
    fprintf(ref, "WARNING: illegal index array caught in glDrawElements; call will be ignored\\.\n");
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_INT, i); /* legal */
#ifdef GL_EXT_draw_range_elements
    if (BUGLE_GL_EXT_draw_range_elements)
    {
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 500, GL_UNSIGNED_INT, NULL);
        fprintf(ref, "WARNING: illegal index array caught in glDrawRangeElements; call will be ignored\\.\n");
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, i); /* legal */
    }
#endif
    glDisableClientState(GL_VERTEX_ARRAY);

    glTexCoordPointer(3, GL_FLOAT, 0, v);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
    fprintf(ref, "WARNING: illegal index array caught in glDrawElements; call will be ignored\\.\n");
#ifdef GL_ARB_multitexture
    if (BUGLE_GL_ARB_multitexture)
    {
        glClientActiveTextureARB(GL_TEXTURE1_ARB);
        glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
        fprintf(ref, "WARNING: illegal index array caught in glDrawElements; call will be ignored\\.\n");
        glClientActiveTextureARB(GL_TEXTURE0_ARB);
    }
#endif
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void invalid_direct_vbo(void)
{
#ifdef GL_ARB_vertex_buffer_object
    if (BUGLE_GL_ARB_vertex_buffer_object)
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
        fprintf(ref, "WARNING: illegal index array caught in glDrawElements; call will be ignored\\.\n");
        glDrawElements(GL_POINTS, 4, GL_UNSIGNED_INT, NULL); /* legal */
#ifdef GL_EXT_draw_range_elements
        if (BUGLE_GL_EXT_draw_range_elements)
        {
            glDrawRangeElementsEXT(GL_POINTS, 0, 0, 500, GL_UNSIGNED_INT, NULL);
            fprintf(ref, "WARNING: illegal index array caught in glDrawRangeElements; call will be ignored\\.\n");
            glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, NULL); /* legal */
        }
#endif
#ifdef GL_EXT_multi_draw_arrays
        if (BUGLE_GL_EXT_multi_draw_arrays)
        {
            GLsizei count = 500;
            const GLvoid *indices = NULL;
            glMultiDrawElementsEXT(GL_POINTS, &count, GL_UNSIGNED_INT, &indices, 1);
            fprintf(ref, "WARNING: illegal index array caught in glMultiDrawElements; call will be ignored\\.\n");

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
    fprintf(ref, "WARNING: illegal vertex array caught in glDrawArrays; call will be ignored\\.\n");
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_INT, i);
    fprintf(ref, "WARNING: illegal vertex array caught in glDrawElements; call will be ignored\\.\n");
#ifdef GL_EXT_draw_range_elements
    if (BUGLE_GL_EXT_draw_range_elements)
    {
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, i);
        fprintf(ref, "WARNING: illegal vertex array caught in glDrawRangeElements; call will be ignored\\.\n");
    }
#endif
#ifdef GL_EXT_multi_draw_arrays
    if (BUGLE_GL_EXT_multi_draw_arrays)
    {
        GLint first = 0;
        GLsizei count = 1;
        const GLvoid *indices = &i;
        glMultiDrawArraysEXT(GL_POINTS, &first, &count, 1);
        fprintf(ref, "WARNING: illegal vertex array caught in glMultiDrawArrays; call will be ignored\\.\n");
        glMultiDrawElementsEXT(GL_POINTS, &count, GL_UNSIGNED_INT, &indices, 1);
        fprintf(ref, "WARNING: illegal vertex array caught in glMultiDrawElements; call will be ignored\\.\n");
    }
#endif

    glDisableClientState(GL_VERTEX_ARRAY);
}

static void invalid_range(void)
{
#ifdef GL_EXT_draw_range_elements
    GLfloat v[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    GLuint i[4] = {0, 1, 0, 1};
    glVertexPointer(3, GL_FLOAT, 0, v);
    glEnableClientState(GL_VERTEX_ARRAY);

    if (BUGLE_GL_EXT_draw_range_elements)
    {
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, i);
        fprintf(ref, "WARNING: glDrawRangeElements indices fall outside range; call will be ignored\\.\n");
        glDrawRangeElementsEXT(GL_POINTS, 1, 1, 4, GL_UNSIGNED_INT, i);
        fprintf(ref, "WARNING: glDrawRangeElements indices fall outside range; call will be ignored\\.\n");
        glDrawRangeElementsEXT(GL_POINTS, 0, 0xffffff, 4, GL_UNSIGNED_INT, i);
        fprintf(ref, "WARNING: illegal vertex array caught in glDrawRangeElements; call will be ignored\\.\n");
    }
#endif
}

int main(int argc, char **argv)
{
    ref = fdopen(3, "w");
    if (!ref) ref = fopen("/dev/null", "w");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("invalid address generator");
    bugle_init();
    invalid_direct();
    invalid_direct_vbo();
    invalid_indirect();
    invalid_range();
    fclose(ref);
    return 0;
}
