/* Uses PBOs with a number of functions that take PBOs, together with the logger.
 * If we don't handle this right, it can crash the app.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef GL_EXT_pixel_buffer_object

static GLuint pbo_ids[2];
static GLvoid *offset = (GLvoid *) (size_t) 4;
static FILE *ref;

static void generate_pbos()
{
    GLsizei pbo_size = 512 * 512 * 4 + 4;
    glGenBuffersARB(2, pbo_ids);
    fprintf(ref, "trace\\.call: glGenBuffersARB\\(2, %p -> { %u, %u }\\)\n", pbo_ids, pbo_ids[0], pbo_ids[1]);
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, pbo_ids[0]);
    fprintf(ref, "trace\\.call: glBindBufferARB\\(GL_PIXEL_PACK_BUFFER(_EXT|_ARB)?, %u\\)\n", pbo_ids[0]);
    glBufferDataARB(GL_PIXEL_PACK_BUFFER_EXT, pbo_size, NULL, GL_DYNAMIC_READ_ARB);
    fprintf(ref, "trace\\.call: glBufferDataARB\\(GL_PIXEL_PACK_BUFFER(_EXT|_ARB)?, %u, \\(nil\\), GL_DYNAMIC_READ(_ARB)?\\)\n", pbo_size);
    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, pbo_ids[1]);
    fprintf(ref, "trace\\.call: glBindBufferARB\\(GL_PIXEL_UNPACK_BUFFER(_EXT|_ARB)?, %u\\)\n", pbo_ids[1]);
    glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, 512 * 512 * 4 + 4, NULL, GL_DYNAMIC_DRAW_ARB);
    fprintf(ref, "trace\\.call: glBufferDataARB\\(GL_PIXEL_UNPACK_BUFFER(_EXT|_ARB)?, %u, \\(nil\\), GL_DYNAMIC_DRAW(_ARB)?\\)\n", pbo_size);
}

static void source_pbo()
{
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, offset);
    fprintf(ref, "trace\\.call: glTexImage2D\\(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 4\\)\n");
}

static void sink_pbo()
{
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, offset);
    fprintf(ref, "trace\\.call: glGetTexImage\\(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 4\\)\n");
    glReadPixels(0, 0, 300, 300, GL_RGBA, GL_UNSIGNED_BYTE, offset);
    fprintf(ref, "trace\\.call: glReadPixels\\(0, 0, 300, 300, GL_RGBA, GL_UNSIGNED_BYTE, 4\\)\n");
}

int main(int argc, char **argv)
{
    ref = fdopen(3, "w");
    if (!ref) ref = fopen("/dev/null", "w");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("object generator");

    glewInit();

    if (!GLEW_EXT_pixel_buffer_object
        && !GLEW_ARB_pixel_buffer_object
        && !GLEW_VERSION_2_1) return 0;

    generate_pbos();
    source_pbo();
    sink_pbo();

    return 0;
}

#else

int main()
{
    return 0;
}
#endif
