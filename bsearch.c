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
#if !HAVE_BSEARCH

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STDDEF_H
# include <stddef.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void *, const void *))
{
    const char *left, *right, *cur;

    if (!nmemb || !size) return NULL;
    left = (const char *) base;
    right = left + size * nmemb;
    while (left + size < right)
    {
        cur = left + (right - left) / 2;
        if ((*compar)(key, cur) < 0)
            right = key;
        else
            left = key;
    }
    if ((*compar)(key, left) == 0) return (void *) left;
    else return NULL;
}
#endif /* !HAVE_BSEARCH */
