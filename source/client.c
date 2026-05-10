#include "../include/client.h"
#include "../commonlib/include/buffer_implementation.h"
#include "../include/constants.h"
#include "../include/random.h"

#include <netinet/in.h>
#include <unistd.h>

struct ConstValue g_short_response_stream = ZERO_INIT;
const struct Client *g_short_response_stream_owner = NULL;

void clients_shuffle(struct ClientBuffer *clients)
{
    /* Fisher-Yates shuffle */
    size_t i;
    for (i = 0; i < clients->size; i++) clients->p[i].shuffle_index = i;
    if (clients->size == 0) return;
    for (i = clients->size - 1; i >= 1; i--)
    {
        size_t j, shuffle_index;
        struct Client *client_i, *client_j;
        j = fair_rand(i + 1);
        client_i = &clients->p[i];
        client_j = &clients->p[j];
        shuffle_index = client_i->shuffle_index;
        client_i->shuffle_index = client_j->shuffle_index;
        client_j->shuffle_index = shuffle_index;
    }
}

void clients_remove_finalized(struct ClientBuffer *clients, struct PollBuffer *polls)
{
    struct Client *client_i, *surviving_client_i;
    struct pollfd *poll_i, *surviving_poll_i;
    for (surviving_client_i = client_i = clients->p, surviving_poll_i = poll_i = polls->p + ACCEPTING_SOCKETS; client_i < clients->p + clients->size; client_i++, poll_i++)
    {
        if (poll_i->fd < 0) continue;
        if (client_i == g_short_response_stream_owner)
        {
            g_short_response_stream_owner -= (size_t)(client_i - surviving_client_i); /* Short buffer owner is guaranteed to not be alive */
        }    
        if (surviving_client_i != client_i)
        {
            *surviving_client_i = *client_i;
            *surviving_poll_i = *poll_i;
        }
        surviving_client_i++;
        surviving_poll_i++;
    }
    clients->size -= (size_t)(client_i - surviving_client_i); /* This is correct, no resize, no finalizers called */
    polls->size -= (size_t)(poll_i - surviving_poll_i);
}

void client_finalize(struct Client *client)
{
    if (client == g_short_response_stream_owner)
    {
        const struct ConstValue zero = ZERO_INIT;
        g_short_response_stream = zero;
        g_short_response_stream_owner = NULL;
        processor_free();
    }
    ring_finalize(&client->request_stream);
    ring_finalize(&client->response_queue);
    ring_finalize(&client->response_stream);
}

void poll_finalize(struct pollfd *poll)
{
    if (poll->fd >= 0) { close(poll->fd); poll->fd = -1; }
}

static void clients_finalize_element(struct Client *client) { client_finalize(client); }
static void polls_finalize_element(struct pollfd *poll) { poll_finalize(poll); }

/* IMPLEMENT_BUFFER_APPEND(struct Client, ClientBuffer, clients_) */
struct Error *clients_append(struct ClientBuffer *buffer, const struct Client *data, size_t size)
{
    struct Error *error;
    bool short_response_stream_present = g_short_response_stream_owner != NULL;
    size_t short_response_stream_index;
    if (short_response_stream_present) short_response_stream_index = (size_t)(g_short_response_stream_owner - buffer->p);
    switch (sizeof(*buffer->p))
    {
    case 1: error = generic_buffer_append_1(buffer, data, size); break;
    case 2: error = generic_buffer_append_2(buffer, data, size); break;
    case 4: error = generic_buffer_append_4(buffer, data, size); break;
    case 8: error = generic_buffer_append_8(buffer, data, size); break;
    default: error = generic_buffer_append_n(buffer, data, size, sizeof(*buffer->p)); break;
    }
    if (short_response_stream_present) g_short_response_stream_owner = &buffer->p[short_response_stream_index];
    return error;
}
IMPLEMENT_BUFFER_FINALIZE(struct Client, ClientBuffer, clients_)

IMPLEMENT_BUFFER_APPEND(struct pollfd, PollBuffer, polls_)
IMPLEMENT_BUFFER_FINALIZE(struct pollfd, PollBuffer, polls_)
