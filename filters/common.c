/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/glfuncs.h"
#include "src/canon.h"
#include "common/safemem.h"
#include "common/bool.h"

/* Invoke filter-set */

static bool invoke_callback(function_call *call, const callback_data *data)
{
    invoke(call);
    return true;
}

static bool initialise_invoke(filter_set *handle)
{
    register_filter(handle, "invoke", invoke_callback);
    register_filter_set_depends("invoke", "procaddress");
    return true;
}

/* glXGetProcAddress wrapper. This ensures that glXGetProcAddressARB
 * returns pointers to our library, not the real library. It is careful
 * to never change the truth value of the return.
 */

static bool procaddress_callback(function_call *call, const callback_data *data)
{
    void (*sym)(void);

    /* FIXME: some systems don't prototype glXGetProcAddressARB (it is,
     * after all, an extension). That means extensions will probably
     * not be intercepted.
     */
#ifdef CFUNC_glXGetProcAddressARB
    switch (canonical_call(call))
    {
    case CFUNC_glXGetProcAddressARB:
        if (!*call->typed.glXGetProcAddressARB.retn) break;
        sym = (void (*)(void))
            get_filter_set_symbol(NULL, (const char *) *call->typed.glXGetProcAddressARB.arg0);
        if (sym) *call->typed.glXGetProcAddressARB.retn = sym;
        break;
    }
#endif
    return true;
}

static bool initialise_procaddress(filter_set *handle)
{
    register_filter(handle, "procaddress", procaddress_callback);
    register_filter_depends("procaddress", "invoke");
    register_filter_depends("trace", "procaddress");
    return true;
}

/* General */

void initialise_filter_library(void)
{
    const filter_set_info invoke_info =
    {
        "invoke",
        initialise_invoke,
        NULL,
        NULL,
        0,
        0
    };
    const filter_set_info procaddress_info =
    {
        "procaddress",
        initialise_procaddress,
        NULL,
        NULL,
        0,
        0
    };
    register_filter_set(&invoke_info);
    register_filter_set(&procaddress_info);
}
