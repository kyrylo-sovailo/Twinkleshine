#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/language.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/random.h"
#include "../../include/ring.h"
#include "../../include/utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRLF "\r\n"
#define ENDLINE "\r\n" /* Specification says CR is optional */

static struct Error *guppy_insert_markers(struct CharBuffer *buffer, unsigned int *first_chunk, unsigned int *chunk_number)
{
    const size_t initial_payload_size = buffer->size;
    size_t chunk_total_sizes[2] = { 0, 0 };
    unsigned int seq = 6 + (unsigned int)random_rand(0x1000000);
    size_t total_size = 0, payload_size = 0;
    char extra[64];
    *first_chunk = seq;
    for (; ; seq++)
    {
        const size_t chunk_extra_size = int_length(seq) + ((total_size == 0) ? 1 : 2);
        const size_t max_chunk_payload_size = CHUNK_SIZE - chunk_extra_size;
        const size_t chunk_payload_size = (initial_payload_size - payload_size < max_chunk_payload_size) ? (initial_payload_size - payload_size) : max_chunk_payload_size;
        const size_t chunk_total_size = chunk_extra_size + chunk_payload_size;
        chunk_total_sizes[0] = chunk_total_sizes[1];
        chunk_total_sizes[1] = chunk_total_size;
        payload_size += chunk_payload_size;
        total_size += chunk_total_size;
        if (chunk_payload_size == 0) break;
    }
    *chunk_number = seq - *first_chunk + 1;
    PRET(string_resize(buffer, total_size));
    for (; ; seq--)
    {
        const size_t chunk_extra_size = (size_t)sprintf(extra, (seq == *first_chunk) ? "%u " : "%u" CRLF, seq);
        const size_t chunk_total_size = chunk_total_sizes[1];
        const size_t chunk_payload_size = chunk_total_size - chunk_extra_size;
        chunk_total_sizes[1] = chunk_total_sizes[0];
        chunk_total_sizes[0] = CHUNK_SIZE;
        payload_size -= chunk_payload_size;
        total_size -= chunk_total_size;
        memmove(buffer->p + total_size + chunk_extra_size, buffer->p + payload_size, chunk_payload_size);
        memcpy(buffer->p + total_size, extra, chunk_extra_size);
        if (seq == *first_chunk) break;
    }
    return OK;
}

struct ExError processor_process_guppy(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstantValue zero = ZERO_INIT;
    union Value method, resource, protocol;
    
    *response_stream = zero;
    response_stream->parts[0].p = "4 Invalid resource" CRLF;
    response_stream->parts[0].size = strlen(response_stream->parts[0].p);

    EXPRETF(ring_get(request_stream, &request->method, false, &method.m), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource.m), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol.m), EEF_CLOSE_LOG_DIE);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &method.c, &resource.c, &protocol.c)); /* Just send 404 as one error package, no one cares */

    return EXOK;
}

struct ExError processor_fixed_guppy(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstantValue zero = ZERO_INIT;

    processor_fixed_guppy_failsafe(fixed, response_stream);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->size, false, 0, 0, &zero, &zero, &zero));

    return EXOK;
}

void processor_fixed_guppy_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream)
{
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    response_stream->p = "4 Maximum number of clients reached" CRLF;        break;
    case FR_MAX_MEMORY:                     response_stream->p = "4 Maximum memory usage reached" CRLF;             break;
    case FR_MAX_UTILIZATION:                response_stream->p = "4 Maximum processor utilization reached" CRLF;    break;
    case FR_UNKNOWN:                        response_stream->p = "4 Unknown error" CRLF;                            break;
    case FR_REQUEST_INVALID:                response_stream->p = "4 Parser error" CRLF;                             break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   response_stream->p = "4 Received too large chunk of data" CRLF;         break;
    case FR_MAX_REQUEST_HEADER_SIZE:        response_stream->p = "4 Actual header size is too large" CRLF;          break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       response_stream->p = "4 Promised content size is too large" CRLF;       break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    response_stream->p = "4 Request incomplete for too long" CRLF;          break;
    default:                                response_stream->p = "4 Unknown error" CRLF;                            break;
    }
    response_stream->size = strlen(response_stream->p);
}

struct Error *processor_print_guppy(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    switch (style)
    {
    case ES_INITIALIZE: /* Overload, no code */
        PRET(string_copy_mem(context->one, STRING_STRLEN("text/gemini" CRLF)));
        context->two->size = 0;
        break;

    case ES_FINALIZE: /* Overload, insert sequence numbers */
        PRET(guppy_insert_markers(context->one, &context->first_chunk, &context->chunk_number));
        break;

    case ES_INTERNAL_REFERENCE: /* Overload, different protocol */
        PRET(string_print_append(context->one, "=> guppy://" DOMAIN_NAME GUPPY_PORT_STRING "/%s%s ", resource, language_question(context->reference_language)));
        PRET(string_vprint_append(context->one, format, va));
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        break;

    default: /* Inherit */
        PRET(processor_print_gemini(context, style, resource, format, va));
        break;
    }
    return OK;
}
