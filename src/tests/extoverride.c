/* Tests the extoverride filterset by checking for certain extensions */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <string.h>
#include "test.h"

test_status extoverride_suite(void)
{
    const char *version = (const char *) glGetString(GL_VERSION);
    const char *exts = (const char *) glGetString(GL_EXTENSIONS);
    /* Bypass GLEW, since it will do its own checks for whether the extension
     * is enabled.
     */
    PFNGLTEXIMAGE3DPROC tex_image_3d = (PFNGLTEXIMAGE3DPROC) test_get_proc_address("glTexImage3D");
    PFNGLTEXIMAGE3DEXTPROC tex_image_3d_ext = (PFNGLTEXIMAGE3DEXTPROC) test_get_proc_address("glTexImage3DEXT");

    if (!tex_image_3d || !tex_image_3d_ext)
        return TEST_SKIPPED;

    TEST_ASSERT(0 == strcmp(version, "1.1"));
    TEST_ASSERT(0 == strcmp(exts, "GL_EXT_texture"));

    /* Try calling a disabled function, to provoke a warning */
    tex_image_3d(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("\\[NOTICE\\] extoverride\\.warn: glTexImage3D was called, although the corresponding extension was suppressed\n");
    tex_image_3d_ext(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("\\[NOTICE\\] extoverride\\.warn: glTexImage3DEXT was called, although the corresponding extension was suppressed\n");

    return TEST_RAN;
}
