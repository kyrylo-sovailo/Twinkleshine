#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/parser.h"
#include "../../include/random.h"
#include "../../include/ring.h"
#include "../../include/tables.h"

#include <stdbool.h>
#include <string.h>

#define FAKE "\tfake\t(NULL)\t0\r\n"
#define CRLF "\r\n"
#define FAKE_CRLF FAKE CRLF

static const char gopher_template_index_page[] =
"iThis is Kyrylo's website." FAKE_CRLF
"iIt is displaying correctly." FAKE_CRLF
"iThe style is not a bug. It is a feature." FAKE_CRLF
"iServed to you by the Twinkleshine server, fully written in C by yours truly." FAKE_CRLF
"ihttps://github.com/kyrylo-sovailo/Twinkleshine" FAKE_CRLF
"iFun fact: %s" FAKE_CRLF;

static void processor_set_locations_one(const struct ValueLocation *request_location, struct ValueLocation *response_location, size_t *size)
{
    response_location->offset = *size;
    response_location->size = request_location->size;
    *size += request_location->size;
}

static void processor_set_locations(const struct Request *request, struct Response *response)
{
    const struct ValueLocation zero = ZERO_INIT;
    response->size = 0;
    response->method = zero;
    response->protocol = zero;
    processor_set_locations_one(&request->resource, &response->resource, &response->size);
}

static struct Error *processor_push_metadata_one(const struct ValueLocation *request_location, const struct Ring *request_stream, struct Ring *response_queue) NODISCARD;
static struct Error *processor_push_metadata_one(const struct ValueLocation *request_location, const struct Ring *request_stream, struct Ring *response_queue)
{
    struct Value value;
    unsigned char i;
    
    PRET(ring_get(request_stream, request_location, false, &value));
    for (i = 0; i < VALUE_PARTS; i++)
    {
        PRET(ring_push_write(response_queue, value.parts[i].size, value.parts[i].p));
    }
    return OK;
}

static struct Error *processor_push_metadata(const struct Request *request, const struct Ring *request_stream, struct Ring *response_queue) NODISCARD;
static struct Error *processor_push_metadata(const struct Request *request, const struct Ring *request_stream, struct Ring *response_queue)
{
    PRET(processor_push_metadata_one(&request->resource, request_stream, response_queue));
    return OK;
}

struct ExError processor_process_gopher(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstValue zero = ZERO_INIT;
    struct Value resource;
    
    /* Actual logic */
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource), EEF_CLOSE_LOG_DIE);
    if (value_compare_case_mem(&resource, "", strlen("")) || value_compare_case_mem(&resource, "/", strlen("/")))
    {
        EXPRETF(string_print(&g_internal_buffer_one, gopher_template_index_page, fun_facts[random_rand(sizeof(fun_facts)/sizeof(*fun_facts))]), EEF_CLOSE_LOG);
        *response_stream = zero;
        response_stream->parts[0].p = g_internal_buffer_one.p;
        response_stream->parts[0].size = g_internal_buffer_one.size;
    }
    else
    {
        *response_stream = zero;
        response_stream->parts[0].p = "3Invalid resource" FAKE_CRLF;
        response_stream->parts[0].size = strlen(response_stream->parts[0].p);
    }

    /* Response */
    processor_set_locations(request, response);
    response->keep_alive = false;
    response->stream_size = g_internal_buffer_one.size;

    /* Metadata */
    EXPRETF(ring_reserve(response_queue, response_queue->size + sizeof(struct Response) + response->size), EEF_CLOSE_LOG);
    EXPRETF(ring_push_write(response_queue, sizeof(struct Response), (const char*)response), EEF_CLOSE_LOG_DIE);
    EXPRETF(processor_push_metadata(request, request_stream, response_queue), EEF_CLOSE_LOG_DIE);

    return EXOK;
}

struct ExError processor_fixed_gopher(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Response zero = ZERO_INIT;

    /* Actual logic */
    processor_fixed_gopher_failsafe(fixed, response_stream);

    /* Response */
    *response = zero;
    response->keep_alive = false;
    response->stream_size = value_const_size(response_stream);

    /* Metadata */
    EXPRETF(ring_reserve(response_queue, response_queue->size + sizeof(struct Response) + response->size), EEF_CLOSE_LOG);
    EXPRETF(ring_push_write(response_queue, sizeof(struct Response), (const char*)response), EEF_CLOSE_LOG_DIE);
    /* TODO: some metadata? */

    return EXOK;
}

void processor_fixed_gopher_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream)
{
    const struct ConstValue zero = ZERO_INIT;
    const char *fixed_string;
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    fixed_string = "3Maximum number of clients reached" FAKE_CRLF;      break;
    case FR_MAX_MEMORY:                     fixed_string = "3Maximum memory usage reached" FAKE_CRLF;           break;
    case FR_MAX_UTILIZATION:                fixed_string = "3Maximum processor utilization reached" FAKE_CRLF;  break;
    case FR_UNKNOWN:                        fixed_string = "3Unknown error" FAKE_CRLF;                          break;
    case FR_REQUEST_INVALID:                fixed_string = "3Parser error" FAKE_CRLF;                           break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   fixed_string = "3Received too large chunk of data" FAKE_CRLF;       break;
    case FR_MAX_REQUEST_HEADER_SIZE:        fixed_string = "3Actual header size is too large" FAKE_CRLF;        break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       fixed_string = "3Promised content size is too large" FAKE_CRLF;     break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    fixed_string = "3Request incomplete for too long" FAKE_CRLF;        break;
    default:                                fixed_string = "3Unknown error" FAKE_CRLF;                          break;
    }
    *response_stream = zero;
    response_stream->parts[0].p = fixed_string;
    response_stream->parts[0].size = strlen(fixed_string);
}
