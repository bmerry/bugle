/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009  Bruce Merry
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

/* Framework for running tests */

#ifndef BUGLE_TESTS_TEST_H
#define BUGLE_TESTS_TEST_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/attributes.h>
#include <bugle/porting.h>
#include <budgie/callapi.h>
#include <stdarg.h>

typedef enum
{
    TEST_RAN,       /* Got to the end but needs analysis */
    TEST_FAILED,    /* Test failed for some reason */
    TEST_SKIPPED    /* Test was not run */
} test_status;

#define TEST_FLAG_LOG     1     /* Test writes a log of regexes to match */
#define TEST_FLAG_CONTEXT 2     /* Test requires a GL context */

typedef struct
{
    const char *name;
    unsigned int flags;
    test_status (*fn)(void);
} test_suite;

void test_assert(int cond, const char *file, int line, const char *cond_str);

#define TEST_ASSERT(cond) test_assert((cond), __FILE__, __LINE__, #cond)

/* Output a regex to the reference log, using printf formatting.
 * This function will abort the test if a printf error is detected.
 */
int test_log_printf(const char *format, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 2);
int test_log_vprintf(const char *format, va_list ap) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 0);

/* Queries a function through glXGetProcAddress or equivalent.
 * To portably use this, you must have a current context.
 */

#if TEST_GL
BUDGIEAPIPROC test_get_proc_address(const char *name);
#endif

#endif /* !BUGLE_TESTS_TEST_H */
