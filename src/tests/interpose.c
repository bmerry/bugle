/* This tests that the linker setup was done correctly, in the sense that
 * glXGetProcAddressARB should return the wrapper, rather than anything
 * interposed over the top by the application.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <bugle/bool.h>
#include <budgie/types.h>
#include "test.h"

extern BUDGIEAPIPROC glXGetProcAddressARB(const GLubyte *);

static bugle_bool test_failed = BUGLE_FALSE;

/* If linking is done wrong, this will override the version that
 * glXGetProcAddress returns. We deliberate choose a post-1.1 function so
 * that GLEW will wrap it up and prevent other tests from directly calling
 * this version.
 */
void BUDGIEAPI glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *data)
{
    test_failed = BUGLE_TRUE;
}

static void (BUDGIEAPIP pglDrawRangeElements)(GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *);

test_status interpose_suite(void)
{
    if (strcmp(glGetString(GL_VERSION), "1.2") < 0)
        return TEST_SKIPPED;    /* Won't have a glDrawElements anyway */

    pglDrawRangeElements = (void (BUDGIEAPIP)(GLenum, GLint *)) glXGetProcAddressARB((const GLubyte *) "glDrawRangeElements");

    TEST_ASSERT(pglDrawRangeElements != glDrawRangeElements);
    pglDrawRangeElements(GL_TRIANGLES, 0, 0, 0, GL_UNSIGNED_SHORT, NULL);
    TEST_ASSERT(!test_failed);
    return TEST_RAN;
}
