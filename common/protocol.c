#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "protocol.h"
#include "bool.h"
#include "safemem.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static bool safe_write(int fd, const void *buf, size_t count)
{
    ssize_t out;

    while ((out = write(fd, buf, count)) < (ssize_t) count)
    {
        if (out == 0) return false; /* EOF */
        else if (out == -1)
        {
            if (errno == EINTR) continue;
            else
            {
                perror("write failed");
                exit(1);
            }
        }
        else
        {
            count -= out;
            buf = (const void *)(((const char *) buf) + out);
        }
    }
    return true;
}

static bool safe_read(int fd, void *buf, size_t count)
{
    ssize_t in;

    while ((in = read(fd, buf, count)) < (ssize_t) count)
    {
        if (in == 0) return false; /* EOF */
        else if (in == -1)
        {
            if (errno == EINTR) continue;
            else
            {
                perror("read failed");
                exit(1);
            }
        }
        else
        {
            count -= in;
            buf = (void *)(((char *) buf) + in);
        }
    }
    return true;
}

bool gldb_protocol_send_code(int fd, uint32_t code)
{
    uint32_t code2;

    code2 = TO_NETWORK(code);
    return safe_write(fd, &code2, sizeof(uint32_t));
}

bool gldb_protocol_send_binary_string(int fd, uint32_t len, const char *str)
{
    uint32_t len2;

    /* FIXME: on 64-bit systems, length could in theory be >2^32 */
    len2 = TO_NETWORK(len);
    if (!safe_write(fd, &len2, sizeof(uint32_t))) return false;
    if (!safe_write(fd, str, len)) return false;
    return true;
}

bool gldb_protocol_send_string(int fd, const char *str)
{
    return gldb_protocol_send_binary_string(fd, strlen(str), str);
}

bool gldb_protocol_recv_code(int fd, uint32_t *code)
{
    uint32_t code2;
    if (safe_read(fd, &code2, sizeof(uint32_t)))
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

    if (!safe_read(fd, &len2, sizeof(uint32_t))) return false;
    *len = TO_HOST(len2);
    *data = bugle_malloc(*len + 1);
    if (!safe_read(fd, *data, *len))
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
