#include "../../include/server.h"
#include "../../commonlib/include/error.h"
#include "../../include/client.h"
#include "../../include/constants.h"
#include "../../include/cryptography.h"

#include <sys/ioctl.h>

struct ExError server_receive_data(struct Client *client, int fd, time_t now, bool *connection_exhausted, bool *connection_closed)
{
    const struct ExError EXOK = { OK };
    const size_t old_size = client->request_stream.size;
    int signed_available; size_t available;

    /* Get how much data is available and allocate buffer */
    EXARET0(ioctl(fd, FIONREAD, &signed_available) >= 0, "ioctl() failed", EEF_CLOSE_LOG);
    EXARET0(signed_available >= 0, "ioctl() failed", EEF_CLOSE_LOG);
    available = (size_t)signed_available;
    EXARET0(available <= MAX_AVAILABLE_REQUEST_STREAM, "ioctl() failed", EEF_SEND_CLOSE_LOG(FR_MAX_AVAILABLE_REQUEST_STREAM));
    if (available < MIN_AVAILABLE_REQUEST_STREAM) available = MIN_AVAILABLE_REQUEST_STREAM;
    if (available > MAX_REQUEST_STREAM_SIZE - client->request_stream.size) available = MAX_REQUEST_STREAM_SIZE - client->request_stream.size;

    /* Read data into buffer */
    EXPRET(server_receive_value(&client->request_stream, fd, available, connection_exhausted, connection_closed));
    if (client->request_stream.size > 0) client->last_request_stream_not_empty = now;

    /* Handle cryptography */
    if (client->ssl != NULL)
    {
        EXPRET(cryptography_decrypt(client, old_size));
        if (client->cryptography_state == CS_SHUTDOWN) *connection_exhausted = *connection_closed = true; /* TODO: add this to the "close vs shutdown" file */
    }
    return EXOK;
}
