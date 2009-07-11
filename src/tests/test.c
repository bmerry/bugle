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

/* Framework for running a test that tests the BuGLe log output */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <GL/glut.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include "test.h"

static const test_suite *current_suite;
static int current_asserts;
static int current_asserts_fail;

/* Name of selected suite, or NULL to run all non-log tests */
static const test_suite *chosen_suite;

/* Name of output log file. One log file may be specified */
static const char *log_filename = NULL;
static FILE *log_handle = NULL;

extern test_status pbo_suite(void);
extern test_status string_suite(void);

static const test_suite suites[] =
{
    { "pbo", TEST_FLAG_LOG | TEST_FLAG_CONTEXT, pbo_suite },
    { "string", 0, string_suite }
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
        int dummy_argc = 1;
        char *dummy_argv[] = {"", NULL};

        glutInit(&dummy_argc, dummy_argv);

        glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
        glutInitWindowSize(300, 300);
        glutCreateWindow("BuGLe test suite");
        glewInit();
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
            printf("Test `%s' PASSED (%d assertions)\n", suite->name, current_asserts);
            break;
        case TEST_SKIPPED:
            printf("Test `%s' SKIPPED\n", suite->name);
            break;
        default:
            printf("Test `%s' FAILED (%d of %d assertions failed)\n",
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
    return 0;
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
