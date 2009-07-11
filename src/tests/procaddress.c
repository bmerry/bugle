/* Does miscellaneous sanity checking */

#include "test.h"
#include <stdlib.h>
#include <budgie/types.h>

extern BUDGIEAPIPROC glXGetProcAddress(const GLubyte *name);

void check_procaddress(void)
{
    /* We deliberately raise an error, then check for it via the
     * address glXGetProcAddressARB gives us for glGetError. If we've
     * messed up glXGetProcAddressARB, we won't get the error because
     * the error interception will have eaten it.
     */
    GLenum (BUDGIEAPIP GetError)(void);

    GetError = (GLenum (BUDGIEAPIP)(void)) glXGetProcAddress((const GLubyte *) "glGetError");
    glPopAttrib();
    TEST_ASSERT((*GetError)() == GL_STACK_UNDERFLOW);
}

test_status procaddress_suite(void)
{
    check_procaddress();
    return TEST_RAN;
}
