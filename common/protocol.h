#ifndef BUGLE_COMMON_PROTOCOL_H
#define BUGLE_COMMON_PROTOCOL_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "common/bool.h"
#include <inttypes.h>
#include <sys/types.h>

/* The top bytes are set to make them easier to pick out in dumps, and
 * so that things will break early if we don't have 32 bit-ness.
 */
#define RESP_ANS               0xabcd0000L
#define RESP_BREAK             0xabcd0001L
#define RESP_BREAK_ERROR       0xabcd0002L
#define RESP_STOP              0xabcd0003L
#define RESP_STATE             0xabcd0004L
#define RESP_ERROR             0xabcd0005L
#define RESP_RUNNING           0xabcd0006L
#define RESP_SCREENSHOT        0xabcd0007L
#define RESP_STATE_NODE_BEGIN  0xabcd0008L
#define RESP_STATE_NODE_END    0xabcd0009L

#define REQ_RUN                0xdcba0000L
#define REQ_CONT               0xdcba0001L
#define REQ_STEP               0xdcba0002L
#define REQ_BREAK              0xdcba0003L
#define REQ_BREAK_ERROR        0xdcba0004L
#define REQ_STATE              0xdcba0005L
#define REQ_QUIT               0xdcba0006L
#define REQ_ASYNC              0xdcba0007L
#define REQ_SCREENSHOT         0xdcba0008L
#define REQ_ENABLE_FILTERSET   0xdcba0009L
#define REQ_DISABLE_FILTERSET  0xdcba000aL
#define REQ_STATE_TREE         0xdcba000bL

#define TO_NETWORK(x) (x)
#define TO_HOST(x) (x)

bool gldb_send_code(int fd, uint32_t code);
bool gldb_send_binary_string(int fd, uint32_t len, const char *str);
bool gldb_send_string(int fd, const char *str);
bool gldb_recv_code(int fd, uint32_t *code);
bool gldb_recv_binary_string(int fd, uint32_t *len, char **data);
bool gldb_recv_string(int fd, char **str);

#endif /* BUGLE_COMMON_PROTOCOL_H */
