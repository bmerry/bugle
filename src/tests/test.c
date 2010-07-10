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

/* Framework for running a test that tests the BuGLe log output */

#if HAVE_CONFIG_H
# include <config.h>
#endif
/* Note: stdlib.h must be included before glut.h under MSVC, otherwise it
 * attempts to redefine exit.
 */
#include <stdlib.h>
#if TEST_GL
# include <GL/glew.h>
# include <GL/glut.h>
#endif
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include "test.h"

static const test_suite *current_suite;
static int current_asserts;
static int current_asserts_fail;
static int tests_fail = 0;

/* Name of selected suite, or NULL to run all non-log tests */
static const test_suite *chosen_suite;

/* Name of output log file. One log file may be specified */
static const char *log_filename = NULL;
static FILE *log_handle = NULL;

#if TEST_GL
extern test_status dlopen_suite(void);
extern test_status errors_suite(void);
extern test_status extoverride_suite(void);
extern test_status interpose_suite(void);
extern test_status pbo_suite(void);
extern test_status pointers_suite(void);
extern test_status procaddress_suite(void);
extern test_status queries_suite(void);
extern test_status setstate_suite(void);
extern test_status showextensions_suite(void);
extern test_status texcomplete_suite(void);
extern test_status triangles_suite(void);
#endif /* TEST_GL */

extern test_status math_suite(void);
extern test_status string_suite(void);
extern test_status threads_suite(void);

static const test_suite suites[] =
{
#if TEST_GL
    { "dlopen",         TEST_FLAG_LOG,                     dlopen_suite },
    { "errors",         TEST_FLAG_CONTEXT,                 errors_suite },
    { "extoverride",    TEST_FLAG_LOG | TEST_FLAG_CONTEXT, extoverride_suite },
    { "interpose",      TEST_FLAG_CONTEXT,                 interpose_suite },
    { "pbo",            TEST_FLAG_LOG | TEST_FLAG_CONTEXT, pbo_suite },
    { "pointers",       TEST_FLAG_LOG | TEST_FLAG_CONTEXT, pointers_suite },
    { "procaddress",    TEST_FLAG_CONTEXT,                 procaddress_suite },
    { "queries",        TEST_FLAG_LOG | TEST_FLAG_CONTEXT, queries_suite },
    { "setstate",       TEST_FLAG_LOG | TEST_FLAG_CONTEXT, setstate_suite },
    { "showextensions", TEST_FLAG_LOG | TEST_FLAG_CONTEXT, showextensions_suite },
    { "texcomplete",    TEST_FLAG_LOG | TEST_FLAG_CONTEXT, texcomplete_suite },
    { "triangles",      TEST_FLAG_LOG | TEST_FLAG_CONTEXT, triangles_suite },
#endif /* TEST_GL */
    { "math",           0,                                 math_suite },
    { "string",         0,                                 string_suite },
    { "threads",        0,                                 threads_suite }
};

static void usage(void)
{
    unsigned int i;
    fprintf(stderr, "Usage: logtester [--test <suite>] [--log <referencelog>]\n");
    fprintf(stderr, "Available suites:\n");
    fprintf(stderr, "\n");
    for (i = 0; i < sizeof(suites) / sizeof(suites[0]); i++)
    {
        fprintf(stderr, "    %s\n", suites[i].name);
    }
    fprintf(stderr, "\n");
}

static void usage_error(const char *format, ...) BUGLE_ATTRIBUTE_FORMAT_PRINTF(1, 2);
static void usage_error(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    usage();
    exit(2);
}

static const test_suite *find_suite(const char *name)
{
    unsigned int i;
    for (i = 0; i < sizeof(suites) / sizeof(suites[0]); i++)
    {
        if (0 == strcmp(suites[i].name, name))
            return &suites[i];
    }
    return NULL;
}

static void process_options(int argc, char **argv)
{
    int i;

    chosen_suite = NULL;
    log_filename = NULL;

    for (i = 1; i < argc; i++)
    {
        if (0 == strcmp(argv[i], "--help"))
        {
            usage();
            exit(0);
        }
        else if (0 == strcmp(argv[i], "--test"))
        {
            if (chosen_suite != NULL)
                usage_error("Cannot specify --test more than once\n");
            if (i + 1 >= argc)
                usage_error("--test requires an argument\n");

            chosen_suite = find_suite(argv[i + 1]);
            if (chosen_suite == NULL)
                usage_error("Test '%s' not found\n", argv[i + 1]);
            i++;
        }
        else if (0 == strcmp(argv[i], "--log"))
        {
            if (log_filename != NULL)
                usage_error("Cannot specify --log more than once\n");
            if (i + 1 >= argc)
                usage_error("--log requires an argument\n");

            log_filename = argv[i + 1];
            i++;
        }
        else
        {
            usage_error("Unknown option '%s'\n", argv[i]);
        }
    }
}

static void init_tests(unsigned int unified_flags)
{
    if (unified_flags & TEST_FLAG_LOG)
    {
        if (log_filename == NULL)
        {
            fprintf(stderr, "--log must be given when running a log test\n");
            exit(1);
        }
        log_handle = fopen(log_filename, "w");
        if (!log_handle)
        {
            int err = errno;
            fprintf(stderr, "Failed to open '%s'", log_filename);
            errno = err;
            perror("");
            exit(1);
        }
    }

    if (unified_flags & TEST_FLAG_CONTEXT)
    {
#if TEST_GL
        int dummy_argc = 1;
        char *dummy_argv[] = {"", NULL};

        glutInit(&dummy_argc, dummy_argv);

        glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
        glutInitWindowSize(300, 300);
        glutCreateWindow("BuGLe test suite");
        glewInit();
#else /* TEST_GL */
        fprintf(stderr, "Test suite was not built with GL support\n");
        exit(1);
#endif /* !TESTGL */
    }
}

static void uninit_tests(unsigned int unified_flags)
{
    if (unified_flags & TEST_FLAG_LOG)
    {
        int ret = fclose(log_handle);
        if (ret != 0)
        {
            int err = errno;
            fprintf(stderr, "Failed to close '%s'", log_filename);
            errno = err;
            perror("");
            exit(1);
        }
    }
}

static void run_test(const test_suite *suite, void *dummy)
{
    test_status result;

    current_suite = suite;
    current_asserts = 0;
    current_asserts_fail = 0;

    result = current_suite->fn();

    if (current_asserts_fail != 0)
        result = TEST_FAILED;

    if (suite->flags & TEST_FLAG_LOG)
    {
        /* Write a machine readable report for caller to process */
        switch (result)
        {
        case TEST_RAN:
            printf("RAN\n");
            break;
        case TEST_SKIPPED:
            printf("SKIPPED\n");
            break;
        default:
            tests_fail++;
            printf("FAILED\n");
            break;
        }
    }
    else
    {
        /* A non-log test should make assertions unless it was skipped; if it
         * didn't, something is probably wrong
         */
        if (result == TEST_RAN && current_asserts == 0)
            result = TEST_FAILED;

        /* Write a human readable report */
        switch (result)
        {
        case TEST_RAN:
            printf("PASSED  Test `%s' (%d assertions)\n", suite->name, current_asserts);
            break;
        case TEST_SKIPPED:
            printf("SKIPPED Test `%s'\n", suite->name);
            break;
        default:
            tests_fail++;
            printf("FAILED  Test `%s' (%d of %d assertions failed)\n",
                   suite->name, current_asserts_fail, current_asserts);
            break;
        }
    }
}

static void unify_flags(const test_suite *suite, void *arg)
{
    unsigned int *flags = (unsigned int *) arg;

    *flags |= suite->flags;
}

static void foreach_test(void (*callback)(const test_suite *suite, void *), void *arg)
{
    if (chosen_suite != NULL)
    {
        callback(chosen_suite, arg);
    }
    else
    {
        unsigned int i;
        for (i = 0; i < sizeof(suites) / sizeof(suites[0]); i++)
            if (!(suites[i].flags & TEST_FLAG_LOG))
                callback(&suites[i], arg);
    }
}

int main(int argc, char **argv)
{
    int unified_flags = 0;

    process_options(argc, argv);
    foreach_test(unify_flags, &unified_flags);

    init_tests(unified_flags);
    foreach_test(run_test, NULL);
    uninit_tests(unified_flags);
    return tests_fail != 0 ? 1 : 0;
}

/* Functions called by the test suites - see logtester.h */

int test_log_printf(const char *format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = test_log_vprintf(format, ap);
    va_end(ap);
    return ret;
}

int test_log_vprintf(const char *format, va_list ap)
{
    int ret;

    ret = vfprintf(log_handle, format, ap);
    if (ret < 0)
    {
        int err = errno;
        fprintf(stderr, "Error writing to '%s'", log_filename);
        errno = err;
        perror("");
        exit(1);
    }
    return ret;
}

void test_assert(int cond, const char *file, int line, const char *cond_str)
{
    current_asserts++;
    if (!cond)
    {
        current_asserts_fail++;
        printf("%s %s:%d: Assertion `%s' FAILED\n",
               current_suite->name, file, line, cond_str);
    }
}
