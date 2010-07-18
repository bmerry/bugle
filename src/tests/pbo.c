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

#ifdef GL_EXT_pixel_buffer_object

static GLuint pbo_ids[2];
static GLvoid *offset = (GLvoid *) (size_t) 4;

static void generate_pbos(void)
{
    GLsizei pbo_size = 512 * 512 * 4 + 4;
    glGenBuffersARB(2, pbo_ids);
    test_log_printf("trace\\.call: glGenBuffersARB\\(2, %p -> { %u, %u }\\)\n", pbo_ids, pbo_ids[0], pbo_ids[1]);
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, pbo_ids[0]);
    test_log_printf("trace\\.call: glBindBufferARB\\(GL_PIXEL_PACK_BUFFER(_EXT|_ARB)?, %u\\)\n", pbo_ids[0]);
    glBufferDataARB(GL_PIXEL_PACK_BUFFER_EXT, pbo_size, NULL, GL_DYNAMIC_READ_ARB);
    test_log_printf("trace\\.call: glBufferDataARB\\(GL_PIXEL_PACK_BUFFER(_EXT|_ARB)?, %u, NULL, GL_DYNAMIC_READ(_ARB)?\\)\n", pbo_size);
    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, pbo_ids[1]);
    test_log_printf("trace\\.call: glBindBufferARB\\(GL_PIXEL_UNPACK_BUFFER(_EXT|_ARB)?, %u\\)\n", pbo_ids[1]);
    glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, 512 * 512 * 4 + 4, NULL, GL_DYNAMIC_DRAW_ARB);
    test_log_printf("trace\\.call: glBufferDataARB\\(GL_PIXEL_UNPACK_BUFFER(_EXT|_ARB)?, %u, NULL, GL_DYNAMIC_DRAW(_ARB)?\\)\n", pbo_size);
}

static void source_pbo(void)
{
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, offset);
    test_log_printf("trace\\.call: glTexImage2D\\(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 4\\)\n");
}

static void sink_pbo(void)
{
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, offset);
    test_log_printf("trace\\.call: glGetTexImage\\(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 4\\)\n");
    glReadPixels(0, 0, 300, 300, GL_RGBA, GL_UNSIGNED_BYTE, offset);
    test_log_printf("trace\\.call: glReadPixels\\(0, 0, 300, 300, GL_RGBA, GL_UNSIGNED_BYTE, 4\\)\n");
}

test_status pbo_suite(void)
{
    if (!GLEW_EXT_pixel_buffer_object
        && !GLEW_ARB_pixel_buffer_object
        && !GLEW_VERSION_2_1)
    {
        printf("No pixel buffer object extension found, skipping test\n");
        return TEST_SKIPPED;
    }

    generate_pbos();
    source_pbo();
    sink_pbo();

    return TEST_RAN;
}

#else

test_status pbo_suite(void)
{
    printf("No compile-time support for GL_EXT_pixel_buffer_object, skipping test\n");
    return TEST_SKIPPED;
}

#endif
