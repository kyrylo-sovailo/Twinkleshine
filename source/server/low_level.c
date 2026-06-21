#include "../../include/server.h"
#include "../../commonlib/include/error.h"
#include "../../include/constants.h"
#include "../../include/ring.h"
#include "../../include/value.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <errno.h>

struct Error *server_send_stream(struct ConstantValue *stream, int fd, enum ConnectionFlag *flags)
{
    /* connection_saturated is a bit redundant because one may just check value size, but I'll keep the flag style to be similar with receive_value() */
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        ssize_t signed_sent;
        if (stream->parts[i].size == 0) continue;
        signed_sent = send(fd, stream->parts[i].p, stream->parts[i].size, 0);
        if (signed_sent > 0)
        {
            const size_t sent = (size_t)signed_sent;
            stream->parts[i].p += sent;
            stream->parts[i].size -= sent;
            if (stream->parts[i].size > 0) { *flags |= CF_SATURATED; break; }
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

struct ExError server_send_message(struct ConstantValue *message, int fd, const struct sockaddr *address, unsigned int first_chunk, union Value *chunks, size_t last_chunk_size)
{
    const struct ExError EXOK = { OK };
    const socklen_t address_size = (address == NULL || address->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
    const unsigned int chunk_number = (unsigned int)value_size(&chunks->c);
    const unsigned int last_chunk = first_chunk + chunk_number - 1;
    const unsigned int second_last_chunk = first_chunk + chunk_number - 2;
    const size_t second_last_chunk_size = value_size(message) - last_chunk_size - CHUNK_SIZE * (chunk_number - 2);
    unsigned int chunk = first_chunk, sent = 0, acknowledged = 0;
    struct ConstantValue local_message = *message;
    unsigned char i;

    for (i = 0; i < VALUE_PARTS; i++)
    {
        signed char *p;
        for (p = (signed char*)chunks->m.parts[i].p; p < (signed char*)chunks->m.parts[i].p + chunks->m.parts[i].size; p++, chunk++)
        {
            const size_t chunk_size = (chunk == second_last_chunk) ? second_last_chunk_size : ((chunk == last_chunk) ? last_chunk_size : CHUNK_SIZE);
            char buffer[CHUNK_SIZE];
            struct ConstantValue chunk; struct ConstantContinuousValue continuous_chunk;
            value_first_second(&local_message, &chunk, chunk_size);
            value_to_continuous(&chunk, &continuous_chunk, buffer);
            
            EXARET(*p < CHUNK_SEND_REPEATS, EEF_SHUTDOWN_LOG);
            if (*p < 0) { acknowledged++; continue; } /*Acknowledged, don't send */
            if (address == NULL) continue; /* Dry run, don't send */
            (void)sendto(fd, continuous_chunk.p, chunk_size, 0, address, address_size);
            (*p)++;
            sent++;
            if (sent == CHUNK_SEND_SIMULTANEOUS) return EXOK;
        }
    }
    if (acknowledged == chunk_number)
    {
        for (i = 0; i < VALUE_PARTS; i++) message->parts[i].size = 0;
    }
    return EXOK;
}

struct ExError server_receive_stream(struct Ring *stream, int fd, size_t size, enum ConnectionFlag *flags)
{
    const struct ExError EXOK = { OK };
    union Value value;
    unsigned char i;

    /* Get buffer */
    EXPRETF(ring_reserve(stream, stream->size + size), EEF_CLOSE_LOG);
    EXPRETF(ring_push_get(stream, size, &value.m), EEF_CLOSE_LOG_DIE);

    /* Receive */
    for (i = 0; i < VALUE_PARTS; i++)
    {
        ssize_t signed_received;
        if (value.m.parts[i].size == 0) continue;
        signed_received = read(fd, value.m.parts[i].p, value.m.parts[i].size);
        if (signed_received == 0) { *flags |= CF_CLOSED; break; }
        if (signed_received > 0)
        {
            const size_t received = (size_t)signed_received;
            value.m.parts[i].p += received;
            value.m.parts[i].size -= received;
            if (value.m.parts[i].size > 0) { *flags |= CF_EXHAUSTED; break; }
        }
        else
        {
            EXARET0(errno == EAGAIN || errno == EWOULDBLOCK, "read() failed", EEF_CLOSE_LOG);
            *flags |= CF_EXHAUSTED;
            break;
        }
    }

    /* Check for unused buffer */
    EXPRETF(ring_unpush(stream, value_size(&value.c)), EEF_CLOSE_LOG_DIE);
    return EXOK;
}
