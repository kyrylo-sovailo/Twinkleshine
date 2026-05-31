#include "../../include/server.h"
#include "../../commonlib/include/error.h"
#include "../../include/value.h"
#include "../../include/ring.h"

#include <sys/socket.h>
#include <unistd.h>

#include <errno.h>

struct Error *server_send_value(struct ConstValue *value, int fd, enum ConnectionFlag *flags)
{
    /* connection_saturated is a bit redundant because one may just check value size, but I'll keep the flag style to be similar with receive_value() */
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        ssize_t signed_sent;
        if (value->parts[i].size == 0) continue;
        signed_sent = send(fd, value->parts[i].p, value->parts[i].size, 0);
        if (signed_sent > 0)
        {
            const size_t sent = (size_t)signed_sent;
            value->parts[i].p += sent;
            value->parts[i].size -= sent;
            if (value->parts[i].size > 0) { *flags |= CF_SATURATED; break; }
        }
        else
        {
            ARET0(errno == EAGAIN || errno == EWOULDBLOCK, "send() failed");
            *flags |= CF_SATURATED;
            break;
        }
    }
    return OK;
}

struct ExError server_receive_value(struct Ring *ring, int fd, size_t size, enum ConnectionFlag *flags)
{
    const struct ExError EXOK = { OK };
    struct Value value;
    unsigned char i;

    /* Get buffer */
    EXPRETF(ring_reserve(ring, ring->size + size), EEF2_CLOSE_LOG);
    EXPRETF(ring_push_get(ring, size, &value), EEF2_CLOSE_LOG_DIE);

    /* Receive */
    for (i = 0; i < VALUE_PARTS; i++)
    {
        ssize_t signed_received;
        if (value.parts[i].size == 0) continue;
        signed_received = read(fd, value.parts[i].p, value.parts[i].size);
        if (signed_received == 0) { *flags |= CF_CLOSED; break; }
        if (signed_received > 0)
        {
            const size_t received = (size_t)signed_received;
            value.parts[i].p += received;
            value.parts[i].size -= received;
            if (value.parts[i].size > 0) { *flags |= CF_EXHAUSTED; break; }
        }
        else
        {
            EXARET0(errno == EAGAIN || errno == EWOULDBLOCK, "read() failed", EEF2_CLOSE_LOG);
            *flags |= CF_EXHAUSTED;
            break;
        }
    }

    /* Check for unused buffer */
    EXPRETF(ring_unpush(ring, value_size(&value)), EEF2_CLOSE_LOG_DIE);
    return EXOK;
}
