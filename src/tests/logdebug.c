/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2012  Bruce Merry
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

/* Tests the logdebug extension */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <bugle/bool.h>
#include <string.h>
#include "test.h"

static const char *test_message = "Message to test logdebug filter-set";

static void logdebug_log_test(void)
{
    if (GLEW_ARB_debug_output)
    {
        glDebugMessageInsertARB(
            GL_DEBUG_SOURCE_APPLICATION_ARB,
            GL_DEBUG_TYPE_OTHER_ARB,
            123,
            GL_DEBUG_SEVERITY_HIGH_ARB,
            -1,
            test_message);
        test_log_printf("logdebug\\.message: Message to test logdebug filter-set \\[source: application type: other id: 123\\]\n");
    }
    else
    {
        test_skipped("ARB_debug_output not found");
    }
}

static void GLAPIENTRY logdebug_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    GLvoid *userParam)
{
    bugle_bool *success = (bugle_bool *) userParam;

    if (source == GL_DEBUG_SOURCE_APPLICATION_ARB
        && type == GL_DEBUG_TYPE_OTHER_ARB
        && id == 123
        && severity == GL_DEBUG_SEVERITY_HIGH_ARB
        && length == strlen(test_message)
        && memcmp(message, test_message, strlen(test_message)) == 0)
        *success = BUGLE_TRUE;
}

static void logdebug_passthrough_test(void)
{
    bugle_bool success = BUGLE_FALSE;
    if (GLEW_ARB_debug_output)
    {
        glDebugMessageCallbackARB(logdebug_callback, &success);
        glDebugMessageInsertARB(
            GL_DEBUG_SOURCE_APPLICATION_ARB,
            GL_DEBUG_TYPE_OTHER_ARB,
            123,
            GL_DEBUG_SEVERITY_HIGH_ARB,
            -1,
            "Message to test logdebug filter-set");
        test_log_printf("logdebug\\.message: Message to test logdebug filter-set \\[source: application type: other id: 123\\]\n");

        glFinish();
        TEST_ASSERT(success);
        glDebugMessageCallbackARB(NULL, NULL);
    }
    else
    {
        test_skipped("ARB_debug_output not found");
    }
}

void logdebug_suite_register(void)
{
    test_suite *ts;

    ts = test_suite_new("logdebug", TEST_FLAG_CONTEXT | TEST_FLAG_LOG, NULL, NULL);
    test_suite_add_test(ts, "log", logdebug_log_test);
    test_suite_add_test(ts, "passthrough", logdebug_passthrough_test);
}
