/* Tests the extoverride filterset by checking for certain extensions */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <GL/gl.h>
#include <string.h>
#include "test.h"

test_status extoverride_suite(void)
{
    const char *version = (const char *) glGetString(GL_VERSION);
    const char *exts = (const char *) glGetString(GL_EXTENSIONS);

    TEST_ASSERT(0 == strcmp(version, "1.1"));
    TEST_ASSERT(0 == strcmp(exts, "GL_EXT_texture"));

    /* Try calling a disabled function, to provoke a warning */
    glTexImage3DEXT(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("\\[NOTICE\\] extoverride\\.warn: glTexImage3DEXT was called, although the corresponding extension was suppressed\n");

    return TEST_RAN;
}
