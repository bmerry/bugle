/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/bool.h>
#include <bugle/filters.h>
#include <bugle/gl/glutils.h>
#include <bugle/glwin/glwin.h>
#include <budgie/types.h>
#include <budgie/addresses.h>
#include <budgie/reflect.h>

/* Invoke filter-set */

static bugle_bool invoke_callback(function_call *call, const callback_data *data)
{
    budgie_invoke(call);
    return BUGLE_TRUE;
}

static bugle_bool invoke_initialise(filter_set *handle)
{
    filter *f;
    f = bugle_filter_new(handle, "invoke");
    bugle_filter_catches_all(f, BUGLE_FALSE, invoke_callback);
    return BUGLE_TRUE;
}

/* glXGetProcAddress wrapper. This ensures that glXGetProcAddressARB
 * returns pointers to our library, not the real library. It is careful
 * to never change the truth value of the return.
 */

static bugle_bool procaddress_callback(function_call *call, const callback_data *data)
{
    /* Note: we can depend on this extension at least on Linux, since it
     * is specified as part of the ABI.
     */
#ifdef BUGLE_GLWIN_GET_PROC_ADDRESS
    BUDGIEAPIPROC sym;
    budgie_function func;

    if (!*call->BUGLE_GLWIN_GET_PROC_ADDRESS.retn) return BUGLE_TRUE;
    func = budgie_function_id((const char *) *call->BUGLE_GLWIN_GET_PROC_ADDRESS.arg0);
    sym = (func == NULL_FUNCTION) ? NULL : budgie_function_address_wrapper(func);
    if (sym) *(BUDGIEAPIPROC *) call->BUGLE_GLWIN_GET_PROC_ADDRESS.retn = sym;
#endif
    return BUGLE_TRUE;
}

static bugle_bool procaddress_initialise(filter_set *handle)
{
    filter *f;

#ifndef STRINGIFY
# define STRINGIFY2(x) #x
# define STRINGIFY(x) STRINGIFY2(x)
#endif
#ifdef BUGLE_GLWIN_GET_PROC_ADDRESS
    f = bugle_filter_new(handle, "procaddress");
    bugle_filter_order("invoke", "procaddress");
    bugle_filter_order("procaddress", "trace");
    bugle_filter_catches(f, STRINGIFY(BUGLE_GLWIN_GET_PROC_ADDRESS), BUGLE_FALSE, procaddress_callback);
#endif
    return BUGLE_TRUE;
}

/* General */

void bugle_initialise_filter_library(void)
{
    static const filter_set_info invoke_info =
    {
        "invoke",
        invoke_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* no documentation */
    };
    static const filter_set_info procaddress_info =
    {
        "procaddress",
        procaddress_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* no documentation */
    };
    bugle_filter_set_new(&invoke_info);
    bugle_filter_set_new(&procaddress_info);

    bugle_filter_set_depends("invoke", "procaddress");
}
