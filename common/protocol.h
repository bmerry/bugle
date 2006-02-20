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
#define RESP_ANS               0xabcd0000UL
#define RESP_BREAK             0xabcd0001UL
#define RESP_BREAK_ERROR       0xabcd0002UL
#define RESP_STOP              0xabcd0003UL
#define RESP_STATE             0xabcd0004UL
#define RESP_ERROR             0xabcd0005UL
#define RESP_RUNNING           0xabcd0006UL
#define RESP_SCREENSHOT        0xabcd0007UL
#define RESP_STATE_NODE_BEGIN  0xabcd0008UL
#define RESP_STATE_NODE_END    0xabcd0009UL
#define RESP_DATA              0xabcd000aUL

#define REQ_RUN                0xdcba0000UL
#define REQ_CONT               0xdcba0001UL
#define REQ_STEP               0xdcba0002UL
#define REQ_BREAK              0xdcba0003UL
#define REQ_BREAK_ERROR        0xdcba0004UL
#define REQ_STATE              0xdcba0005UL
#define REQ_QUIT               0xdcba0006UL
#define REQ_ASYNC              0xdcba0007UL
#define REQ_SCREENSHOT         0xdcba0008UL
#define REQ_ACTIVATE_FILTERSET 0xdcba0009UL
#define REQ_DEACTIVATE_FILTERSET  0xdcba000aUL
#define REQ_STATE_TREE         0xdcba000bUL
#define REQ_DATA               0xdbca000cUL

#define REQ_DATA_TEXTURE       0xedbc0000UL
#define REQ_DATA_SHADER        0xedbc0001UL

#define TO_NETWORK(x) (x)
#define TO_HOST(x) (x)

bool gldb_protocol_send_code(int fd, uint32_t code);
bool gldb_protocol_send_binary_string(int fd, uint32_t len, const char *str);
bool gldb_protocol_send_string(int fd, const char *str);
bool gldb_protocol_recv_code(int fd, uint32_t *code);
bool gldb_protocol_recv_binary_string(int fd, uint32_t *len, char **data);
bool gldb_protocol_recv_string(int fd, char **str);

#endif /* BUGLE_COMMON_PROTOCOL_H */
