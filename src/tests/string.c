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

/* Validate the API porting layer string functions */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/string.h>
#include "test.h"

static void string_snprintf(void)
{
    char buffer[10];
    int ret;

    /* Non-truncated */
    ret = bugle_snprintf(buffer, sizeof(buffer), "abc");
    TEST_ASSERT(ret == 3);

    /* Full, non-truncated */
    ret = bugle_snprintf(buffer, sizeof(buffer), "abcdefghi");
    TEST_ASSERT(ret == 9);

    /* Just truncated */
    ret = bugle_snprintf(buffer, sizeof(buffer), "abcdefghij");
    TEST_ASSERT(ret == 10);

    /* Truncated */
    ret = bugle_snprintf(buffer, sizeof(buffer), "abcdefghijklm");
    TEST_ASSERT(ret == 13);

    /* Null buffer */
    ret = bugle_snprintf(NULL, 0, "abcdefghijklm");
    TEST_ASSERT(ret == 13);
}

test_status string_suite(void)
{
    string_snprintf();

    return TEST_RAN;
}
