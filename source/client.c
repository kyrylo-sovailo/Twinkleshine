#include "../include/client.h"
#include "../include/buffer_implementation.h"
#include "../include/request_processor.h"

#include <unistd.h>

#include <string.h>

void client_finalize(struct Client *client)
{
    if (client->short_output_buffer.size > 0) { memset(&client->short_output_buffer, 0, sizeof(struct ValuePart)); request_processor_free(); }
    ring_finalize(&client->input_buffer);
    ring_finalize(&client->output_buffer);
}

void poll_finalize(struct pollfd *poll)
{
    if (poll->fd >= 0) { close(poll->fd); poll->fd = -1; }
}

IMPLEMENT_BUFFER_RESIZE(struct Client, ClientBuffer, clients_)
IMPLEMENT_BUFFER_APPEND(struct Client, ClientBuffer, clients_)
IMPLEMENT_BUFFER_FINALIZE(struct Client, ClientBuffer, clients_)

IMPLEMENT_BUFFER_RESIZE(struct pollfd, PollBuffer, polls_)
IMPLEMENT_BUFFER_APPEND(struct pollfd, PollBuffer, polls_)
IMPLEMENT_BUFFER_FINALIZE(struct pollfd, PollBuffer, polls_)
