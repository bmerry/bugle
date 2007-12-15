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

#include <stdio.h>
#include <stdlib.h>

#if HAVE_WEAK
extern void bugle_log_xalloc_die(void);

/* Some arbitrary function not used in this file. See lib/lock.h for an
 * explanation of why this is useful.
 */
extern void bugle_log(const char *filterset, const char *event, int severity,
                      const char *message);
# pragma weak bugle_log_xalloc_die
# pragma weak bugle_log
#endif

void xalloc_die(void)
{
#if HAVE_WEAK
    if (&bugle_log != NULL)
        bugle_log_xalloc_die();
    else
#endif
    {
        fprintf(stderr, "bugle: memory allocation failed\n");
    }
    abort();
}
