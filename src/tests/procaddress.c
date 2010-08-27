/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2005, 2009-2010  Bruce Merry
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

/* Checks that GetProcAddress is correctly intercepted */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <GL/glew.h>
#include <stdlib.h>
#include <budgie/callapi.h>
#include "test.h"

static void check_procaddress(void)
{
    /* We deliberately raise an error, then check for it via the
     * address glXGetProcAddressARB gives us for glGetError. If we've
     * messed up glXGetProcAddressARB, we won't get the error because
     * the error interception will have eaten it.
     */
    GLenum (BUDGIEAPIP pglGetError)(void);

    pglGetError = (GLenum (BUDGIEAPIP)(void)) test_get_proc_address("glGetError");
    if (pglGetError == NULL)
    {
        test_skipped("Could not get glGetError address");
        return;
    }

    glPopAttrib();
    TEST_ASSERT((*pglGetError)() == GL_STACK_UNDERFLOW);
}

void procaddress_suite_register(void)
{
    test_suite *ts = test_suite_new("procaddress", TEST_FLAG_CONTEXT, NULL, NULL);
    test_suite_add_test(ts, "procaddress", check_procaddress);
}
