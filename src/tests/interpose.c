/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2007, 2009, 2010  Bruce Merry
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

/* This tests that the linker setup was done correctly, in the sense that
 * glXGetProcAddressARB should return the wrapper, rather than anything
 * interposed over the top by the application.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <bugle/bool.h>
#include <budgie/callapi.h>
#include "test.h"

static bugle_bool failed = BUGLE_FALSE;

/* If linking is done wrong, this will override the version that
 * glXGetProcAddress returns. We deliberately choose a post-1.1 function so
 * that GLEW will wrap it up and prevent other tests from directly calling
 * this version.
 */
#undef glDrawRangeElements
void BUDGIEAPI glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *data)
{
    failed = BUGLE_TRUE;
}

void interpose_test(void)
{
    PFNGLDRAWRANGEELEMENTSPROC pglDrawRangeElements;

    if (strcmp((const char *) glGetString(GL_VERSION), "1.2") < 0)
    {
        test_skipped("GL 1.2 required");
        return;
    }

    pglDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC) test_get_proc_address("glDrawRangeElements");

    TEST_ASSERT(pglDrawRangeElements != glDrawRangeElements);
    failed = BUGLE_FALSE;
    pglDrawRangeElements(GL_TRIANGLES, 0, 0, 0, GL_UNSIGNED_SHORT, NULL);
    TEST_ASSERT(!failed);
}

void interpose_suite_register(void)
{
    test_suite *ts = test_suite_new("interpose", TEST_FLAG_CONTEXT, NULL, NULL);
    test_suite_add_test(ts, "interpose", interpose_test);
}
