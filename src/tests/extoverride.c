/* Tests the extoverride filterset by checking for certain extensions */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define GL_GLEXT_PROTOTYPES 1

#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <string.h>
#include "test.h"

/* If GLEW got pulled in anywhere, prevent it from redefining symbols */

#undef glTexImage3D
#undef glTexImage3DEXT

test_status extoverride_suite(void)
{
    const char *version = (const char *) glGetString(GL_VERSION);
    const char *exts = (const char *) glGetString(GL_EXTENSIONS);

    TEST_ASSERT(0 == strcmp(version, "1.1"));
    TEST_ASSERT(0 == strcmp(exts, "GL_EXT_texture"));

    /* Try calling a disabled function, to provoke a warning */
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("\\[NOTICE\\] extoverride\\.warn: glTexImage3D was called, although the corresponding extension was suppressed\n");
    glTexImage3DEXT(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("\\[NOTICE\\] extoverride\\.warn: glTexImage3DEXT was called, although the corresponding extension was suppressed\n");

    return TEST_RAN;
}
