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
#include "common/safemem.h"
#include "common/bool.h"

/* Invoke filter-set */

static bool invoke_callback(function_call *call, const callback_data *data)
{
    budgie_invoke(call);
    return true;
}

static bool initialise_invoke(filter_set *handle)
{
    filter *f;
    f = bugle_register_filter(handle, "invoke");
    bugle_register_filter_catches_all(f, invoke_callback);
    return true;
}

/* glXGetProcAddress wrapper. This ensures that glXGetProcAddressARB
 * returns pointers to our library, not the real library. It is careful
 * to never change the truth value of the return.
 */

static bool procaddress_callback(function_call *call, const callback_data *data)
{
    /* FIXME: some systems don't prototype glXGetProcAddressARB (it is,
     * after all, an extension). That means extensions will probably
     * not be intercepted.
     */
#ifdef GLX_ARB_get_proc_address
    void (*sym)(void);

    if (!*call->typed.glXGetProcAddressARB.retn) return true;
    sym = (void (*)(void))
        bugle_get_filter_set_symbol(NULL, (const char *) *call->typed.glXGetProcAddressARB.arg0);
    if (sym) *call->typed.glXGetProcAddressARB.retn = sym;
#endif
    return true;
}

static bool initialise_procaddress(filter_set *handle)
{
    filter *f;

#ifdef GLX_ARB_get_proc_address
    f = bugle_register_filter(handle, "procaddress");
    bugle_register_filter_depends("procaddress", "invoke");
    bugle_register_filter_depends("trace", "procaddress");
    bugle_register_filter_catches(f, GROUP_glXGetProcAddressARB, procaddress_callback);
#endif
    return true;
}

/* General */

void bugle_initialise_filter_library(void)
{
    static const filter_set_info invoke_info =
    {
        "invoke",
        initialise_invoke,
        NULL,
        NULL,
        0,
        NULL /* no documentation */
    };
    static const filter_set_info procaddress_info =
    {
        "procaddress",
        initialise_procaddress,
        NULL,
        NULL,
        0,
        NULL /* no documentation */
    };
    bugle_register_filter_set(&invoke_info);
    bugle_register_filter_set(&procaddress_info);

    bugle_register_filter_set_depends("invoke", "procaddress");
}
