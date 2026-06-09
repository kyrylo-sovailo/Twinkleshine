#include "../../include/server.h"
#include "../../commonlib/include/error.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/cryptography.h"

#include <sys/ioctl.h>

#include <stdlib.h>
#include <string.h>

static struct ExError server_receive_data_guppy(struct Client *client, enum ConnectionFlag *flags)
{
    const struct ExError EXOK = { OK };
    const char guppy[] = "guppy://";
    const size_t guppy_size = sizeof(guppy) - 1;
    if (g_short_request_message_owner != client)
    {
        *flags |= CF_SATURATED;
        return EXOK;
    }
    if (g_short_request_message_size >= guppy_size && memcmp(g_short_request_message, guppy, guppy_size) == 0)
    {
        /* Request */
        EXPRETF(ring_reserve(&client->request_stream, client->request_stream.size + g_short_request_message_size), EEF_CLOSE_LOG);
        EXPRETF(ring_push_write(&client->request_stream, g_short_request_message_size, g_short_request_message), EEF_CLOSE_LOG_DIE);
    }
    else
    {
        /* Acknowledgement */
        char *end;
        struct ValueLocation location; struct Value value;
        const signed char sent = -1;
        const unsigned long int seq = strtoul(g_short_request_message, &end, 10);
        EXARET0(memcmp(end, "\r\n", 3) != 0 && seq >= client->response.first_chunk && seq < client->response.first_chunk + client->response.chunks.size,
            "Invalid acknowledgement", EEF_SEND_CLOSE_LOG(FR_UNKNOWN));
        location.offset = sizeof(struct Response) + client->response.chunks.offset + (seq - client->response.first_chunk);
        location.size = 1;
        EXPRETF(ring_get(&client->response_queue, &location, false, &value), EEF_CLOSE_LOG); /* Don't die because seq came from the internet */
        value_write(&value, (const char*)&sent);
    }
    g_short_request_message_size = 0;
    g_short_request_message_owner = NULL;
    return EXOK;
}

struct ExError server_receive_data(struct Client *client, int fd, time_t now, enum ConnectionFlag *flags)
{
    const struct ExError EXOK = { OK };
    const size_t old_size = client->request_stream.size;
    int signed_available; size_t available;

    /* Handle guppy */
    if (ACCEPTING_SOCKET_IS_MESSAGE(client->accepting_socket))
    {
        EXPRET(server_receive_data_guppy(client, flags));
        *flags |= CF_EXHAUSTED;
        if (client->request_stream.size > 0) client->last_request_stream_not_empty = now;
        return EXOK;
    }

    /* Get how much data is available and allocate buffer */
    EXARET0(ioctl(fd, FIONREAD, &signed_available) >= 0, "ioctl() failed", EEF_CLOSE_LOG);
    EXARET0(signed_available >= 0, "ioctl() failed", EEF_CLOSE_LOG);
    available = (size_t)signed_available;
    EXARET0(available <= MAX_AVAILABLE_REQUEST_STREAM, "ioctl() failed", EEF_SEND_CLOSE_LOG(FR_MAX_AVAILABLE_REQUEST_STREAM));
    if (available < MIN_AVAILABLE_REQUEST_STREAM) available = MIN_AVAILABLE_REQUEST_STREAM;
    if (available > MAX_REQUEST_STREAM_SIZE - client->request_stream.size) available = MAX_REQUEST_STREAM_SIZE - client->request_stream.size;

    /* Read data into buffer */
    EXPRET(server_receive_value(&client->request_stream, fd, available, flags));
    if (client->request_stream.size > 0) client->last_request_stream_not_empty = now;

    /* Handle cryptography */
    if (client->ssl != NULL)
    {
        EXPRET(cryptography_decrypt(client, old_size));
    }
    return EXOK;
}
