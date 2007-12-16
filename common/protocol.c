#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "common/protocol.h"
#include "common/misc.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "full-read.h"
#include "full-write.h"
#include "xalloc.h"
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#define TO_NETWORK(x) htonl(x)
#define TO_HOST(x) ntohl(x)

/* Returns false on EOF, aborts on failure */
static bool my_safe_write(int fd, const void *buf, size_t count)
{
    ssize_t out;

    out = full_write(fd, buf, count);
    if (out < count)
    {
        if (errno)
        {
            perror("write failed");
            exit(1);
        }
        return false;
    }
    else
        return true;
}

/* Returns false on EOF, aborts on failure */
static bool my_safe_read(int fd, void *buf, size_t count)
{
    ssize_t out;

    out = full_read(fd, buf, count);
    if (out < count)
    {
        if (errno)
        {
            perror("read failed");
            exit(1);
        }
        return false;
    }
    else
        return true;
}

bool gldb_protocol_send_code(int fd, uint32_t code)
{
    uint32_t code2;

    code2 = TO_NETWORK(code);
    return my_safe_write(fd, &code2, sizeof(uint32_t));
}

bool gldb_protocol_send_binary_string(int fd, uint32_t len, const char *str)
{
    uint32_t len2;

    /* FIXME: on 64-bit systems, length could in theory be >=2^32. This is
     * not as unlikely as it sounds, since textures are retrieved in floating
     * point and so might be several times larger on the wire than on the
     * video card. If this happens, the logical thing is to make a packet size
     * of exactly 2^32-1 signal that a continuation packet will follow, which
     * avoid breaking backwards compatibility. Alternatively, it might be
     * better to just break backwards compatibility.
     */
    len2 = TO_NETWORK(len);
    if (!my_safe_write(fd, &len2, sizeof(uint32_t))) return false;
    if (!my_safe_write(fd, str, len)) return false;
    return true;
}

bool gldb_protocol_send_string(int fd, const char *str)
{
    return gldb_protocol_send_binary_string(fd, strlen(str), str);
}

bool gldb_protocol_recv_code(int fd, uint32_t *code)
{
    uint32_t code2;
    if (my_safe_read(fd, &code2, sizeof(uint32_t)))
    {
        *code = TO_HOST(code2);
        return true;
    }
    else
        return false;
}

bool gldb_protocol_recv_binary_string(int fd, uint32_t *len, char **data)
{
    uint32_t len2;
    int old_errno;

    if (!my_safe_read(fd, &len2, sizeof(uint32_t))) return false;
    *len = TO_HOST(len2);
    *data = xmalloc(*len + 1);
    if (!my_safe_read(fd, *data, *len))
    {
        old_errno = errno;
        free(*data);
        errno = old_errno;
        return false;
    }
    else
    {
        (*data)[*len] = '\0';
        return true;
    }
}

bool gldb_protocol_recv_string(int fd, char **str)
{
    uint32_t dummy;

    return gldb_protocol_recv_binary_string(fd, &dummy, str);
}
