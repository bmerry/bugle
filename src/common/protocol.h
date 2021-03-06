/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2009  Bruce Merry
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

#ifndef BUGLE_COMMON_PROTOCOL_H
#define BUGLE_COMMON_PROTOCOL_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/bool.h>
#include <bugle/export.h>
#include <bugle/io.h>
#include "platform/types.h"

/* The top bytes are set to make them easier to pick out in dumps, and
 * so that things will break early if we don't have 32 bit-ness.
 */
#define RESP_ANS                       0xabcd0000UL
#define RESP_BREAK                     0xabcd0001UL
#define RESP_BREAK_EVENT               0xabcd0002UL
#define RESP_STOP                      0xabcd0003UL  /* Obsolete */
#define RESP_STATE                     0xabcd0004UL  /* Obsolete */
#define RESP_ERROR                     0xabcd0005UL
#define RESP_RUNNING                   0xabcd0006UL
#define RESP_SCREENSHOT                0xabcd0007UL  /* Obsolete */
#define RESP_STATE_NODE_BEGIN          0xabcd0008UL
#define RESP_STATE_NODE_END            0xabcd0009UL
#define RESP_DATA                      0xabcd000aUL
#define RESP_STATE_NODE_BEGIN_RAW_OLD  0xabcd000bUL  /* Obsolete */
#define RESP_STATE_NODE_END_RAW        0xabcd000cUL
#define RESP_STATE_NODE_BEGIN_RAW      0xabcd000dUL

#define REQ_RUN                        0xdcba0000UL
#define REQ_CONT                       0xdcba0001UL
#define REQ_STEP                       0xdcba0002UL
#define REQ_BREAK                      0xdcba0003UL
#define REQ_BREAK_ERROR                0xdcba0004UL  /* Obsolete, replaced by REQ_BREAK_EVENT */
#define REQ_STATE                      0xdcba0005UL  /* Obsolete */
#define REQ_QUIT                       0xdcba0006UL
#define REQ_ASYNC                      0xdcba0007UL
#define REQ_SCREENSHOT                 0xdcba0008UL  /* Obsolete */
#define REQ_ACTIVATE_FILTERSET         0xdcba0009UL
#define REQ_DEACTIVATE_FILTERSET       0xdcba000aUL
#define REQ_STATE_TREE                 0xdcba000bUL
#define REQ_DATA                       0xdcba000cUL
#define REQ_STATE_TREE_RAW_OLD         0xdcba000dUL  /* Obsolete */
#define REQ_STATE_TREE_RAW             0xdcba000eUL
#define REQ_BREAK_EVENT                0xdcba000fUL

#define REQ_DATA_TEXTURE               0xedbc0000UL
#define REQ_DATA_SHADER                0xedbc0001UL
#define REQ_DATA_FRAMEBUFFER           0xedbc0002UL
#define REQ_DATA_INFO_LOG              0xedbc0003UL
#define REQ_DATA_BUFFER                0xedbc0004UL

#define REQ_EVENT_GL_ERROR             0x00000000UL
#define REQ_EVENT_COMPILE_ERROR        0x00000001UL
#define REQ_EVENT_LINK_ERROR           0x00000002UL
/* Count of events - increment as events are added */
#define REQ_EVENT_COUNT                0x00000003UL

BUGLE_EXPORT_PRE bugle_bool gldb_protocol_send_code(bugle_io_writer *writer, bugle_uint32_t code) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE bugle_bool gldb_protocol_send_code64(bugle_io_writer *writer, bugle_uint64_t code) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE bugle_bool gldb_protocol_send_binary_string(bugle_io_writer *writer, bugle_uint32_t len, const char *str) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE bugle_bool gldb_protocol_send_string(bugle_io_writer *writer, const char *str) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE bugle_bool gldb_protocol_recv_code(bugle_io_reader *reader, bugle_uint32_t *code) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE bugle_bool gldb_protocol_recv_code64(bugle_io_reader *reader, bugle_uint64_t *code) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE bugle_bool gldb_protocol_recv_binary_string(bugle_io_reader *reader, bugle_uint32_t *len, char **data) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE bugle_bool gldb_protocol_recv_string(bugle_io_reader *reader, char **str) BUGLE_EXPORT_POST;

#endif /* BUGLE_COMMON_PROTOCOL_H */
