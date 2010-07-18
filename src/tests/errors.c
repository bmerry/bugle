#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include "test.h"

static void invalid_enum(void)
{
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_FLOAT);
    TEST_ASSERT(glGetError() == GL_INVALID_ENUM);
}

static void invalid_operation(void)
{
    glBegin(GL_TRIANGLES);
    glGetError();
    glEnd();
    TEST_ASSERT(glGetError() == GL_INVALID_OPERATION);
}

static void invalid_value(void)
{
    /* border of 2 is illegal */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    TEST_ASSERT(glGetError() == GL_INVALID_VALUE);
}

static void stack_underflow(void)
{
    glPopAttrib();
    TEST_ASSERT(glGetError() == GL_STACK_UNDERFLOW);
}

static void stack_overflow(void)
{
    GLint depth, i;

    glGetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH, &depth);
    for (i = 0; i <= depth; i++)
        glPushAttrib(GL_ALL_ATTRIB_BITS);
    for (i = 0; i < depth; i++)
        glPopAttrib();
    TEST_ASSERT(glGetError() == GL_STACK_OVERFLOW);
}

test_status errors_suite(void)
{
    invalid_enum();
    invalid_operation();
    invalid_value();
    stack_underflow();
    stack_overflow();
    return TEST_RAN;
}
