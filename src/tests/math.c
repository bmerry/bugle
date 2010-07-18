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

/* Validate the API porting layer maths functions */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/math.h>
#include <math.h>
#include <float.h>
#include "test.h"

static void math_isnan(void)
{
    volatile double x = 0.0;

    TEST_ASSERT(bugle_isnan(x / x));
    TEST_ASSERT(!bugle_isnan(0.0));
    TEST_ASSERT(!bugle_isnan(DBL_MAX));
    TEST_ASSERT(!bugle_isnan(HUGE_VAL));
}

static void math_isfinite(void)
{
    TEST_ASSERT(bugle_isfinite(0.0));
    TEST_ASSERT(bugle_isfinite(DBL_MAX));
    TEST_ASSERT(!bugle_isfinite(HUGE_VAL));
    TEST_ASSERT(!bugle_isfinite(-HUGE_VAL));
}

static void math_nan(void)
{
    TEST_ASSERT(bugle_nan() != bugle_nan());
    TEST_ASSERT(bugle_isnan(bugle_nan()));
}

static void math_sinf(void)
{
    TEST_ASSERT(fabs(sin(1.0) - bugle_sinf(1.0f)) < 1e-5);
    TEST_ASSERT(bugle_sinf(0.0f) == 0.0f);
}

static void math_cosf(void)
{
    TEST_ASSERT(fabs(cos(1.0) - bugle_cosf(1.0f)) < 1e-5);
    TEST_ASSERT(bugle_cosf(0.0f) == 1.0f);
}

test_status math_suite(void)
{
    math_isnan();
    math_isfinite();
    math_nan();
    math_cosf();
    math_sinf();

    return TEST_RAN;
}
