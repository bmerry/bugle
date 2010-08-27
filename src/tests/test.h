/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009-2010  Bruce Merry
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
#include <bugle/bool.h>
#include <budgie/callapi.h>
#include <stdarg.h>

typedef enum
{
    TEST_STATUS_FAILED  = 0,   /* Test failed for some reason */
    TEST_STATUS_PASSED  = 1,   /* All assertions passed, no analysis required */
    TEST_STATUS_RAN     = 2,   /* Got to the end but needs analysis */
    TEST_STATUS_SKIPPED = 3,   /* Test was not run due to missing features */
    TEST_STATUS_NOTRUN         /* Hasn't run yet */
} test_status;

#define TEST_FLAG_LOG     1     /* Test writes a log of regexes to match */
#define TEST_FLAG_CONTEXT 2     /* Test requires a GL context */

typedef struct test
{
    struct test *next;
    char *name;
    void (*run)(void);

    test_status status;
    char *reason;
    unsigned int total_assertions;
    unsigned int failed_assertions;
} test;

typedef struct test_suite
{
    struct test_suite *next;
    char *name;
    unsigned int flags;
    void (*setup)(void);
    void (*teardown)(void);
    test *first_test;
    test *last_test;

    bugle_bool enabled;
    unsigned int count_status[TEST_STATUS_NOTRUN];
} test_suite;

int test_assert(int cond, const char *file, int line, const char *cond_str);

/* Checks that condition is true. Does not terminate testing if false.
 * It returns cond.
 */
#define TEST_ASSERT(cond) test_assert((cond), __FILE__, __LINE__, #cond)

/* Output a regex to the reference log, using printf formatting.
 * This function will abort the test if a printf error is detected.
 */
int test_log_printf(const char *format, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 2);
int test_log_vprintf(const char *format, va_list ap) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 0);

/* Indicate that the current test was skipped due to missing features */
void test_skipped(const char *format, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 2);

/* Indicate that the current test failed for some reason other than an
 * assertion */
void test_failed(const char *format, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 2);

/* Full name of the current test */
const char *test_name(void);

/* Create a new test suite and register it */
test_suite *test_suite_new(const char *name, unsigned int flags,
                           void (*setup)(void), void (*teardown)(void));

/* Add a test to a test suite */
void test_suite_add_test(test_suite *suite, const char *name, void (*run)(void));

/* Queries a function through glXGetProcAddress or equivalent.
 * To portably use this, you must have a current context.
 */

#if TEST_GL
BUDGIEAPIPROC test_get_proc_address(const char *name);
#endif

#endif /* !BUGLE_TESTS_TEST_H */
