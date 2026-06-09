#include "../../include/server.h"
#include "../../commonlib/include/error.h"
#include "../../include/constants.h"
#include "../../include/ring.h"
#include "../../include/value.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <errno.h>

struct Error *server_send_value(struct Value *value, int fd, enum ConnectionFlag *flags)
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

struct ExError server_send_message(struct Value *value, int fd, const struct sockaddr *address, unsigned int first_chunk, struct Value *chunks, size_t last_chunk_size)
{
    const struct ExError EXOK = { OK };
    const unsigned int chunk_number = (unsigned int)value_size(chunks);
    const unsigned int last_chunk = first_chunk + chunk_number - 1;
    const unsigned int second_last_chunk = first_chunk + chunk_number - 2;
    const size_t second_last_chunk_size = value_size(value) - last_chunk_size - CHUNK_SIZE * (chunk_number - 2);
    unsigned int chunk = first_chunk, sent = 0, acknowledged = 0;
    struct Value *local_value = value;
    unsigned char i;

    for (i = 0; i < VALUE_PARTS; i++)
    {
        signed char *p;
        for (p = (signed char*)chunks->parts[i].p; p < (signed char*)chunks->parts[i].p + chunks->parts[i].size; p++, chunk++)
        {
            const size_t chunk_size = (chunk == second_last_chunk) ? second_last_chunk_size : ((chunk == last_chunk) ? last_chunk_size : CHUNK_SIZE);
            char buffer[CHUNK_SIZE];
            struct Value chunk; struct ValuePart chunk_part;
            
            EXARET(*p < CHUNK_SEND_REPEATS, EEF_SHUTDOWN_LOG);
            if (*p < 0) { acknowledged++; continue; }
            chunk = *local_value;
            value_first(&chunk, chunk_size);
            value_to_value_part(&chunk_part, &chunk, buffer);
            (void)sendto(fd, chunk_part.p, chunk_size, 0, address, (address->sa_family == AF_INET6) ? sizeof(struct in6_addr) : sizeof(struct in_addr));
            value_second(local_value, chunk_size);
            (*p)++;
            sent++;
            if (sent == CHUNK_SEND_SIMULTANEOUS) return EXOK;
        }
    }
    if (acknowledged == chunk_number)
    {
        for (i = 0; i < VALUE_PARTS; i++) value->parts[i].size = 0;
    }
    return EXOK;
}

struct ExError server_receive_value(struct Ring *ring, int fd, size_t size, enum ConnectionFlag *flags)
{
    const struct ExError EXOK = { OK };
    struct Value value;
    unsigned char i;

    /* Get buffer */
    EXPRETF(ring_reserve(ring, ring->size + size), EEF_CLOSE_LOG);
    EXPRETF(ring_push_get(ring, size, &value), EEF_CLOSE_LOG_DIE);

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
            EXARET0(errno == EAGAIN || errno == EWOULDBLOCK, "read() failed", EEF_CLOSE_LOG);
            *flags |= CF_EXHAUSTED;
            break;
        }
    }

    /* Check for unused buffer */
    EXPRETF(ring_unpush(ring, value_size(&value)), EEF_CLOSE_LOG_DIE);
    return EXOK;
}
