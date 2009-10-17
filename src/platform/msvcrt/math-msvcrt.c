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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/math.h>
#include <math.h>
#include <float.h>
#include <stddef.h>
#include <stdlib.h>

int bugle_isfinite(double x)
{
    return _finite(x);
}

int bugle_isnan(double x)
{
    return _isnan(x);
}

double bugle_round(double x)
{
    return floor(x + 0.5);
}

float bugle_sinf(float x)
{
    return sinf(x);
}

float bugle_cosf(float x)
{
    return cosf(x);
}
