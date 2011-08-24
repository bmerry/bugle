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
/* Required to compile GLUT under MinGW */
# if defined(_WIN32) && !defined(_STDCALL_SUPPORTED)
#  define _STDCALL_SUPPORTED
# endif
# include <GL/glut.h>
#endif
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <bugle/string.h>
#include <bugle/memory.h>
#include "test.h"

static test_suite *current_suite = NULL;
static test *current_test = NULL;

/* Name of selected suite, or NULL to run all non-log tests */
static test_suite *chosen_suite;

/* Name of output log file. One log file may be specified */
static const char *log_filename = NULL;
static FILE *log_handle = NULL;

/* Linked list of registered suites */
static test_suite *first_suite = NULL;
static test_suite *last_suite = NULL;

#if TEST_GL
extern void arbcreatecontext_suite_register(void);
extern void dlopen_suite_register(void);
extern void draw_suite_register(void);
extern void errors_suite_register(void);
extern void extoverride_suite_register(void);
extern void interpose_suite_register(void);
extern void pbo_suite_register(void);
extern void pointers_suite_register(void);
extern void procaddress_suite_register(void);
extern void queries_suite_register(void);
extern void setstate_suite_register(void);
extern void showextensions_suite_register(void);
extern void texcomplete_suite_register(void);
extern void triangles_suite_register(void);
#endif

extern void math_suite_register(void);
extern void string_suite_register(void);
extern void threads_suite_register(void);

static void (* const register_fns[])(void) =
{
#if TEST_GL
    arbcreatecontext_suite_register,
    dlopen_suite_register,
    draw_suite_register,
    errors_suite_register,
    extoverride_suite_register,
    interpose_suite_register,
    pbo_suite_register,
    pointers_suite_register,
    procaddress_suite_register,
    queries_suite_register,
    setstate_suite_register,
    showextensions_suite_register,
    texcomplete_suite_register,
    triangles_suite_register,
#endif /* TEST_GL */
    string_suite_register,
    math_suite_register,
    threads_suite_register
};

static void usage(void)
{
    test_suite *ts;
    fprintf(stderr, "Usage: bugletest [--suite <suite>] [--log <referencelog>]\n");
    fprintf(stderr, "Available suites:\n");
    fprintf(stderr, "\n");
    for (ts = first_suite; ts != NULL; ts = ts->next)
    {
        fprintf(stderr, "    %s\n", ts->name);
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

static test_suite *find_suite(const char *name)
{
    test_suite *s;
    for (s = first_suite; s != NULL; s = s->next)
    {
        if (0 == strcmp(s->name, name))
            return s;
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
        else if (0 == strcmp(argv[i], "--suite"))
        {
            if (chosen_suite != NULL)
                usage_error("Cannot specify --suite more than once\n");
            if (i + 1 >= argc)
                usage_error("--suite requires an argument\n");

            chosen_suite = find_suite(argv[i + 1]);
            if (chosen_suite == NULL)
                usage_error("Suite '%s' not found\n", argv[i + 1]);
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

/* Set the enabled flag for the tests we intend to run */
static void enable_suites(void)
{
    if (chosen_suite != NULL)
    {
        chosen_suite->enabled = BUGLE_TRUE;
    }
    else
    {
        test_suite *ts;
        for (ts = first_suite; ts != NULL; ts = ts->next)
            if (!(ts->flags & TEST_FLAG_LOG))
                ts->enabled = BUGLE_TRUE;
    }
}

static void init_suites(void)
{
    unsigned int all_flags = 0;
    test_suite *s;

    for (s = first_suite; s != NULL; s = s->next)
        if (s->enabled)
            all_flags |= s->flags;

    if (all_flags & TEST_FLAG_LOG)
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

    if (all_flags & TEST_FLAG_CONTEXT)
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

static void uninit_suites(void)
{
    if (log_handle != NULL)
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

static void run_suite(test_suite *suite)
{
    unsigned int i;
    static const char * const labels[TEST_STATUS_NOTRUN] =
    {
        "FAILED",
        "PASSED",
        "RAN",
        "SKIPPED"
    };
    current_suite = suite;

    for (i = 0; i < TEST_STATUS_NOTRUN; i++)
        current_suite->count_status[i] = 0;

    if (current_suite->setup != NULL)
        current_suite->setup();

    for (current_test = suite->first_test; current_test != NULL; current_test = current_test->next)
    {
        current_test->total_assertions = 0;
        current_test->failed_assertions = 0;
        current_test->reason = NULL;
        current_test->status = TEST_STATUS_NOTRUN;

        fprintf(stderr, "  %-50s", current_test->name);
        fflush(stderr);
        current_test->run();
        if (current_test->status == TEST_STATUS_NOTRUN)
        {
            if (current_test->failed_assertions > 0)
                current_test->status = TEST_STATUS_FAILED;
            else if (current_suite->flags & TEST_FLAG_LOG)
                current_test->status = TEST_STATUS_RAN;
            else if (current_test->total_assertions == 0)
                test_skipped("WARNING: no assertions found");
            else
                current_test->status = TEST_STATUS_PASSED;
        }

        if (current_test->reason == NULL)
        {
            switch (current_test->status)
            {
            case TEST_STATUS_PASSED:
                current_test->reason = bugle_asprintf("%d assertions",
                                                      current_test->total_assertions);
                break;
            case TEST_STATUS_FAILED:
                current_test->reason = bugle_asprintf("%d/%d assertions failed",
                                                      current_test->failed_assertions,
                                                      current_test->total_assertions);
                break;
            default:
                break;
            }
        }

        if (log_handle != NULL)
        {
            /* Log test output is parsed by the SConscript */
            printf("%s\n", labels[current_test->status]);
        }

        if (current_test->reason != NULL && current_test->reason[0] != '\0')
        {
            fprintf(stderr, "[%s - %s]\n", labels[current_test->status], current_test->reason);
        }
        else
        {
            fprintf(stderr, "[%s]\n", labels[current_test->status]);
        }

        current_suite->count_status[current_test->status]++;
    }

    if (current_suite->teardown != NULL)
        current_suite->teardown();
}

/* Returns number of failing tests */
int run_all_suites(void)
{
    test_suite *suite;

    int totals[TEST_STATUS_NOTRUN] = {};
    for (suite = first_suite; suite != NULL; suite = suite->next)
    {
        if (suite->enabled)
        {
            int i;
            run_suite(suite);

            for (i = 0; i < TEST_STATUS_NOTRUN; i++)
                totals[i] += suite->count_status[i];
        }
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "%d passed, %d failed, %d ran, %d skipped\n",
           totals[TEST_STATUS_PASSED],
           totals[TEST_STATUS_FAILED],
           totals[TEST_STATUS_RAN],
           totals[TEST_STATUS_SKIPPED]);
    return totals[TEST_STATUS_FAILED];
}

static void create_suites(void)
{
    unsigned int i;
    for (i = 0; i < sizeof(register_fns) / sizeof(register_fns[0]); i++)
    {
        register_fns[i]();
    }
}

int main(int argc, char **argv)
{
    int failed = 0;

    create_suites();
    process_options(argc, argv);

    enable_suites();
    init_suites();
    failed = run_all_suites();
    uninit_suites();

    return failed != 0 ? 1 : 0;
}

/* Functions called by the test suites - see test.h */

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

const char * test_name(void)
{
    return current_test->name;
}

void test_failed(const char *format, ...)
{
    va_list ap;
    char *msg;

    va_start(ap, format);
    msg = bugle_vasprintf(format, ap);
    va_end(ap);
    free(current_test->reason);
    current_test->reason = msg;
    current_test->status = TEST_STATUS_FAILED;
}

void test_skipped(const char *format, ...)
{
    va_list ap;
    char *msg;

    va_start(ap, format);
    msg = bugle_vasprintf(format, ap);
    va_end(ap);
    free(current_test->reason);
    current_test->reason = msg;
    current_test->status = TEST_STATUS_SKIPPED;
}

test_suite *test_suite_new(const char *name, unsigned int flags,
                           void (*setup)(void), void (*teardown)(void))
{
    test_suite *ts;

    ts = BUGLE_ZALLOC(test_suite);
    ts->name = bugle_strdup(name);
    ts->flags = flags;
    ts->setup = setup;
    ts->teardown = teardown;
    if (last_suite == NULL)
        first_suite = last_suite = ts;
    else
    {
        last_suite->next = ts;
        last_suite = ts;
    }
    return ts;
}

void test_suite_add_test(test_suite *suite, const char *name, void (*run)(void))
{
    test *t;

    t = BUGLE_ZALLOC(test);
    t->name = bugle_asprintf("%s::%s", suite->name, name);
    t->run = run;
    t->status = TEST_STATUS_NOTRUN;
    if (suite->last_test == NULL)
        suite->first_test = suite->last_test = t;
    else
    {
        suite->last_test->next = t;
        suite->last_test = t;
    }
}

int test_assert(int cond, const char *file, int line, const char *cond_str)
{
    current_test->total_assertions++;
    if (!cond)
    {
        current_test->failed_assertions++;
        fprintf(stderr, "%s %s:%d: Assertion `%s' FAILED\n",
                current_test->name, file, line, cond_str);
    }
    return cond;
}

#if TEST_GL

#if BUGLE_GLWIN_GLX

#include <GL/glx.h>
#include <GL/glxext.h>
BUDGIEAPIPROC test_get_proc_address(const char *name)
{
    return glXGetProcAddressARB((const GLubyte *) name);
}

#elif BUGLE_GLWIN_WGL

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
BUDGIEAPIPROC test_get_proc_address(const char *name)
{
    return (BUDGIEAPIPROC) wglGetProcAddress(name);
}

#elif BUGLE_GLWIN_EGL

#include <EGL/egl.h>

BUDGIEAPIPROC test_get_proc_address(const char *name)
{
    return eglGetProcAddress(name);
}

#else
# error "Window system not known"
#endif /* BUGLE_GLWIN_* */

#endif /* TEST_GL */
