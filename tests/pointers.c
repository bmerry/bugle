/* Pass a variety of illegal pointers to OpenGL */
#ifdef NDEBUG /* make sure that assert is defined */
# undef NDEBUG
#endif

#define GL_GLEXT_PROTOTYPES
#include <GL/glut.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static void invalid_direct(void)
{
    GLfloat v[3] = {0.0f, 0.0f, 0.0f};
    GLuint i[4] = {0, 0, 0, 0};
    glVertexPointer(3, GL_FLOAT, 0, v);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_INT, i); /* legal */
#ifdef GL_EXT_draw_range_elements
    if (glutExtensionSupported("GL_EXT_draw_range_elements"))
    {
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 500, GL_UNSIGNED_INT, NULL);
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, i); /* legal */
    }
#endif
    glDisableClientState(GL_VERTEX_ARRAY);

    glTexCoordPointer(3, GL_FLOAT, 0, v);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
#ifdef GL_ARB_multitexture
    if (glutExtensionSupported("GL_ARB_multitexture"))
    {
        glClientActiveTexture(GL_TEXTURE1_ARB);
        glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
        glClientActiveTexture(GL_TEXTURE0_ARB);
    }
#endif
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

static void invalid_range(void)
{
#ifdef GL_EXT_draw_range_elements
    GLfloat v[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    GLuint i[4] = {0, 1, 0, 1};
    glVertexPointer(3, GL_FLOAT, 0, v);
    glEnableClientState(GL_VERTEX_ARRAY);

    if (glutExtensionSupported("GL_EXT_draw_range_elements"))
    {
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, i);
        glDrawRangeElementsEXT(GL_POINTS, 1, 1, 4, GL_UNSIGNED_INT, i);
        glDrawRangeElementsEXT(GL_POINTS, 0, 0xffffff, 4, GL_UNSIGNED_INT, i);
    }
#endif
}

static void invalid_direct_vbo(void)
{
#ifdef GL_ARB_vertex_buffer_object
    if (glutExtensionSupported("GL_ARB_vertex_buffer_object"))
    {
        GLfloat v[3] = {0.0f, 0.0f, 0.0f};
        glVertexPointer(3, GL_FLOAT, 0, v);
        glEnableClientState(GL_VERTEX_ARRAY);

        GLuint indices[4] = {0, 0, 0, 0};
        GLuint id;
        glGenBuffersARB(1, &id);
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, id);
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(indices),
                        indices, GL_STATIC_DRAW_ARB);
        glDrawElements(GL_POINTS, 500, GL_UNSIGNED_INT, NULL);
        glDrawElements(GL_POINTS, 4, GL_UNSIGNED_INT, NULL); /* legal */
#ifdef GL_EXT_draw_range_elements
        if (glutExtensionSupported("GL_EXT_draw_range_elements"))
        {
            glDrawRangeElementsEXT(GL_POINTS, 0, 0, 500, GL_UNSIGNED_INT, NULL);
            glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, NULL); /* legal */
        }
#endif

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        glDeleteBuffersARB(1, &id);
        glDisableClientState(GL_VERTEX_ARRAY);
    }
#endif /* GL_ARB_vertex_buffer_object */
}

static void invalid_indirect(void)
{
    GLuint i[4] = {0, 0, 0, 0};

    glVertexPointer(3, GL_FLOAT, 0, NULL);
    glEnableClientState(GL_VERTEX_ARRAY);

    glDrawArrays(GL_POINTS, 0, 500);
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_INT, i);
#ifdef GL_EXT_draw_range_elements
    if (glutExtensionSupported("GL_EXT_draw_range_elements"))
        glDrawRangeElementsEXT(GL_POINTS, 0, 0, 4, GL_UNSIGNED_INT, i);
#endif

    glDisableClientState(GL_VERTEX_ARRAY);
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("invalid address generator");
    invalid_direct();
    invalid_direct_vbo();
    invalid_indirect();
    invalid_range();
    return 0;
}
