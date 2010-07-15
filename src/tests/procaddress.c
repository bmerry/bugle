/* Checks that GetProcAddress is correctly intercepted */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <GL/glew.h>
#include <stdlib.h>
#include <budgie/callapi.h>
#include "test.h"

test_status check_procaddress(void)
{
    /* We deliberately raise an error, then check for it via the
     * address glXGetProcAddressARB gives us for glGetError. If we've
     * messed up glXGetProcAddressARB, we won't get the error because
     * the error interception will have eaten it.
     */
    GLenum (BUDGIEAPIP pglGetError)(void);

    pglGetError = (GLenum (BUDGIEAPIP)(void)) test_get_proc_address("glGetError");
    if (pglGetError == NULL)
        return TEST_SKIPPED; /* Some window system APIs don't return pointers to core functions */

    glPopAttrib();
    TEST_ASSERT((*pglGetError)() == GL_STACK_UNDERFLOW);
    return TEST_RAN;
}

test_status procaddress_suite(void)
{
    return check_procaddress();
}
