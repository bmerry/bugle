/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
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

/* budgie is a utility to generate code for API interception. It is intended
 * to be able to stand alone from bugle, and in particular has no explicit
 * knowledge about OpenGL. However, the design was created with bugle in
 * mind, so it may be difficult to adapt for other APIs.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <list>
#include <string>
#include "tree.h"

#ifndef BUDGIE_BUDGIE_H
#define BUDGIE_BUDGIE_H

enum override_type
{
    OVERRIDE_LENGTH,
    OVERRIDE_TYPE,
    OVERRIDE_DUMP
};

/* Routines to be called by parser */

void parser_limit(const std::string &limit);
void parser_header(const std::string &header);
void parser_library(const std::string &library);
void parser_alias(const std::string &func1, const std::string &func2);

void parser_param(override_type mode,
                  const std::string &regex, int param,
                  const std::string &code);
void parser_type(override_type mode,
                 const std::string &type, const std::string &code);

void parser_extra_type(const std::string &type);
void parser_bitfield(const std::string &newtype,
                     const std::string &base,
                     const std::list<std::string> &bits);

#endif /* !BUDGIE_BUDGIE_H */
