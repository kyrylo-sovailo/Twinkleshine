#include "../../include/server.h"
#include "../../commonlib/include/error.h"
#include "../../include/client.h"
#include "../../include/ring.h"

/* Flushes short output buffer into long output buffer */
static struct ExError client_push_to_long_response_stream(struct Client *client, const struct ConstValue *data) NODISCARD;
static struct ExError client_push_to_long_response_stream(struct Client *client, const struct ConstValue *data)
{
    const struct ExError EXOK = { OK };
    unsigned char i;
    EXPRETF(ring_reserve(&client->response_stream, client->response_stream.size + value_const_size(data)), EEF_CLOSE_LOG);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRETF(ring_push_write(&client->response_stream, data->parts[i].size, data->parts[i].p), EEF_CLOSE_LOG_DIE);
    }
    return EXOK;
}

struct ExError server_process_data(struct Client *client, time_t now)
{
    const struct ExError EXOK = { OK };
    struct ConstValue response_stream;

    /* Parse request, quit if the request is incomplete */
    EXPRET(parser_parse(&client->parser, &client->request, &client->request_stream));
    if (client->parser.state != RPS_END) return EXOK;
    client->last_request_complete = now;

    /* Flush short output buffer to long output buffer because we are about to get another portion of data to send */
    if (g_short_response_stream_owner != NULL)
    {
        const struct ConstValue zero = ZERO_INIT;
        EXPRET(client_push_to_long_response_stream(g_short_response_stream_owner, &g_short_response_stream));
        g_short_response_stream = zero;
        g_short_response_stream_owner = NULL;
        processor_free();
    }

    /* Generate response */
    EXPRET(processor_process(&client->request, &client->request_stream, (client->response_queue.size == 0) ? &client->response : NULL, &client->response_queue, &response_stream));
    EXPRETF(ring_pop(&client->request_stream, client->request.stream_size), EEF_CLOSE_LOG_DIE);

    /* Save the response to either short-term or long-term buffer */
    if (client->response_stream.size == 0)
    {
        g_short_response_stream = response_stream;
        g_short_response_stream_owner = client;
    }
    else
    {
        EXPRET(client_push_to_long_response_stream(client, &response_stream));
        processor_free();
    }

    /* Reset parser for future data */
    if (client->response.keep_alive)
    {
        const struct Request zero = ZERO_INIT;
        const struct Parser zero2 = ZERO_INIT;
        client->request = zero;
        client->parser = zero2;
    }
    return EXOK;
}
