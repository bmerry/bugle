/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005, 2009, 2010  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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

void errors_suite_register(void)
{
    test_suite *ts = test_suite_new("errors", TEST_FLAG_CONTEXT, NULL, NULL);
    test_suite_add_test(ts, "INVALID_ENUM", invalid_enum);
    test_suite_add_test(ts, "INVALID_OPERATION", invalid_operation);
    test_suite_add_test(ts, "INVALID_VALUE", invalid_value);
    test_suite_add_test(ts, "STACK_UNDERFLOW", stack_underflow);
    test_suite_add_test(ts, "STACK_OVERFLOW", stack_overflow);
}
