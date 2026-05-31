#include "../../include/server.h"
#include "../../commonlib/include/error.h"
#include "../../include/client.h"
#include "../../include/cryptography.h"
#include "../../include/ring.h"

/* Flushes short output buffer into long output buffer */
static struct ExError server_reserve_and_push_value(struct Ring *response_stream, const struct ConstValue *data) NODISCARD;
static struct ExError server_reserve_and_push_value(struct Ring *response_stream, const struct ConstValue *data)
{
    const struct ExError EXOK = { OK };
    unsigned char i;
    EXPRETF(ring_reserve(response_stream, response_stream->size + value_const_size(data)), EEF_CLOSE_LOG);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRETF(ring_push_write(response_stream, data->parts[i].size, data->parts[i].p), EEF_CLOSE_LOG_DIE);
    }
    return EXOK;
}

struct ExError server_process_data(struct Client *client, time_t now)
{
    const struct ExError EXOK = { OK };
    struct Response response;
    struct ConstValue response_stream;

    /* Parse request, quit if the request is incomplete */
    EXPRET(parser_parse(&client->parser, &client->request, &client->request_stream));
    if (client->parser.state != RPS_END) return EXOK;
    client->last_request_complete = now;

    /* Flush short output buffer to long output buffer because we are about to get another portion of data to send */
    if (g_short_response_stream_owner != NULL)
    {
        const struct ConstValue zero = ZERO_INIT;
        EXPRET(server_reserve_and_push_value(&g_short_response_stream_owner->response_stream, &g_short_response_stream));
        g_short_response_stream = zero;
        g_short_response_stream_owner = NULL;
        processor_free();
    }

    /* Generate response */
    EXPRET(processor_process(&client->request, &client->request_stream, &response, &client->response_queue, &response_stream)); /* TODO: response queue is the odd one out */
    EXPRETF(ring_pop(&client->request_stream, client->request.stream_size), EEF_CLOSE_LOG_DIE);
    if (client->response_count == 0) client->response = response;

    /* Save the response to either short-term or long-term buffer */
    if (client->ssl != NULL)
    {
        EXPRET(cryptography_encrypt(client, &response, &response_stream));
        processor_free();
    }
    else if (client->response_stream.size > 0)
    {
        EXPRET(server_reserve_and_push_value(&client->response_stream, &response_stream));
        processor_free();
    }
    else
    {
        g_short_response_stream = response_stream;
        g_short_response_stream_owner = client;
    }
    client->response_count++;

    /* Reset parser for future data */
    if (client->response.keep_alive)
    {
        const struct Request zero = ZERO_INIT;
        const struct Parser zero2 = ZERO_INIT;
        client->request = zero;
        client->parser = zero2;
    }
    else
    {
        EXPRET(cryptography_finalize(client));
    }
    return EXOK;
}
