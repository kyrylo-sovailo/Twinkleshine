#include "../../include/processor.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/parser.h"
#include "../../include/ring.h"

#include <string.h>

struct CharBuffer g_internal_buffer_one = ZERO_INIT;
struct CharBuffer g_internal_buffer_two = ZERO_INIT;

static struct ExError processor_specific(unsigned char accepting_socket, const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;
static struct ExError processor_specific(unsigned char accepting_socket, const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream)
{
    const struct ExError EXOK = { OK };
    if (ACCEPTING_SOCKET_IS_HTTP(accepting_socket))             { EXPRET(processor_process_http(request, request_stream, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_HTTPS(accepting_socket))       { EXPRET(processor_process_http(request, request_stream, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_GOPHER(accepting_socket))      { EXPRET(processor_process_gopher(request, request_stream, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_FINGER(accepting_socket))      { EXPRET(processor_process_finger(request, request_stream, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_GEMINI(accepting_socket))      { EXPRET(processor_process_gemini(request, request_stream, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_SPARTAN(accepting_socket))     { EXPRET(processor_process_spartan(request, request_stream, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_NEX(accepting_socket))         { EXPRET(processor_process_nex(request, request_stream, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_TEXT(accepting_socket))        { EXPRET(processor_process_text(request, request_stream, response, response_queue, response_stream)); }
    else /* if (ACCEPTING_SOCKET_IS_GUPPY(accepting_socket)) */ { EXPRET(processor_process_guppy(request, request_stream, response, response_queue, response_stream)); }
    return EXOK;
}

struct Error *processor_module_initialize(void)
{
    PRET(processor_module_initialize_http());
    return OK;
}

void processor_module_finalize(void)
{
    processor_module_initialize_http();
    string_finalize(&g_internal_buffer_one);
    string_finalize(&g_internal_buffer_two);
}

void processor_free(void)
{
    /* Do nothing */
}

struct ExError processor_construct_response(struct Response *response, struct Ring *response_queue,
    size_t stream_size, bool keep_alive, unsigned int first_chunk, unsigned int chunk_number,
    const struct ConstantValue *method, const struct ConstantValue *resource, const struct ConstantValue *protocol)
{
    const struct ExError EXOK = { OK };
    struct MutableValue chunks_value;
    unsigned char i;

    response->stream_size = stream_size;
    response->keep_alive = keep_alive;
    response->first_chunk = first_chunk;
    response->silent = false;
    response->size = 0;

    response->method.offset = response->size;
    response->method.size = value_size(method);
    response->size += response->method.size;

    response->resource.offset = response->size;
    response->resource.size = value_size(resource);
    response->size += response->resource.size;

    response->protocol.offset = response->size;
    response->protocol.size = value_size(protocol);
    response->size += response->protocol.size;

    response->chunks.offset = response->size;
    response->chunks.size = chunk_number;
    response->size += response->chunks.size;

    EXPRETF(ring_reserve(response_queue, response_queue->size + sizeof(struct Response) + response->size), EEF_SEND_SHUTDOWN_LOG(FR_UNKNOWN));
    EXPRETF(ring_push_write(response_queue, sizeof(struct Response), (const char*)response), PARTIAL_EEF_DIE);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRETF(ring_push_write(response_queue, method->parts[i].size, method->parts[i].p), PARTIAL_EEF_DIE);
    }
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRETF(ring_push_write(response_queue, resource->parts[i].size, resource->parts[i].p), PARTIAL_EEF_DIE);
    }
    for (i = 0; i < VALUE_PARTS; i++)
    {
        EXPRETF(ring_push_write(response_queue, protocol->parts[i].size, protocol->parts[i].p), PARTIAL_EEF_DIE);
    }
    EXPRETF(ring_push_get(response_queue, response->chunks.size, &chunks_value), PARTIAL_EEF_DIE);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        memset(chunks_value.parts[i].p, 0, chunks_value.parts[i].size);
    }
    return EXOK;
}

struct ExError processor_process(unsigned char accepting_socket, const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const bool http = ACCEPTING_SOCKET_IS_HTTP(accepting_socket) || ACCEPTING_SOCKET_IS_HTTPS(accepting_socket);
    union Value method;

    EXPRETF(ring_get(request_stream, &request->method, false, &method.m), EEF_CLOSE_LOG_DIE);
    if ((!http) || (http && value_compare_case_mem(&method.c, STRING_STRLEN("get"))))
    {
        /* Generic text printing */
        struct ProcessorPrintContext context = { &g_internal_buffer_one, &g_internal_buffer_two, request, 0, 0, '\0', '\0', 0, 0 };
        union Value resource, protocol;
        bool invalid_resource = false;
        EXPRETF(ring_get(request_stream, &request->resource, false, &resource.m), EEF_CLOSE_LOG_DIE);
        EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol.m), EEF_CLOSE_LOG_DIE);
        EXPRETF(processor_generic(accepting_socket, &context, &resource.c, &invalid_resource), EEF_SEND_CLOSE_LOG(FR_UNKNOWN));
        if (!invalid_resource)
        {
            response_stream->parts[0].p = g_internal_buffer_one.p;
            response_stream->parts[0].size = g_internal_buffer_one.size;
            response_stream->parts[1].p = g_internal_buffer_two.p;
            response_stream->parts[1].size = g_internal_buffer_two.size;
            EXPRET(processor_construct_response(response, response_queue,
                value_size(response_stream), request->keep_alive, context.first_chunk, context.chunk_number, &method.c, &resource.c, &protocol.c));
            return EXOK;
        }
    }

    /* Specific response */
    EXPRET(processor_specific(accepting_socket, request, request_stream, response, response_queue, response_stream));
    return EXOK;
}

struct ExError processor_fixed(unsigned char accepting_socket, enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream)
{
    const struct ExError EXOK = { OK };
    if (ACCEPTING_SOCKET_IS_HTTP(accepting_socket))             { EXPRET(processor_fixed_http(fixed, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_HTTPS(accepting_socket))       { EXPRET(processor_fixed_http(fixed, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_GOPHER(accepting_socket))      { EXPRET(processor_fixed_gopher(fixed, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_FINGER(accepting_socket))      { EXPRET(processor_fixed_finger(fixed, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_GEMINI(accepting_socket))      { EXPRET(processor_fixed_gemini(fixed, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_SPARTAN(accepting_socket))     { EXPRET(processor_fixed_spartan(fixed, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_NEX(accepting_socket))         { EXPRET(processor_fixed_nex(fixed, response, response_queue, response_stream)); }
    else if (ACCEPTING_SOCKET_IS_TEXT(accepting_socket))        { EXPRET(processor_fixed_text(fixed, response, response_queue, response_stream)); }
    else /* if (ACCEPTING_SOCKET_IS_GUPPY(accepting_socket)) */ { EXPRET(processor_fixed_guppy(fixed, response, response_queue, response_stream)); }
    return EXOK;
}

void processor_fixed_failsafe(unsigned char accepting_socket, enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream)
{
    if (ACCEPTING_SOCKET_IS_HTTP(accepting_socket))             processor_fixed_http_failsafe(fixed, response_stream);
    else if (ACCEPTING_SOCKET_IS_HTTPS(accepting_socket))       processor_fixed_http_failsafe(fixed, response_stream);
    else if (ACCEPTING_SOCKET_IS_GOPHER(accepting_socket))      processor_fixed_gopher_failsafe(fixed, response_stream);
    else if (ACCEPTING_SOCKET_IS_FINGER(accepting_socket))      processor_fixed_finger_failsafe(fixed, response_stream);
    else if (ACCEPTING_SOCKET_IS_GEMINI(accepting_socket))      processor_fixed_gemini_failsafe(fixed, response_stream);
    else if (ACCEPTING_SOCKET_IS_SPARTAN(accepting_socket))     processor_fixed_spartan_failsafe(fixed, response_stream);
    else if (ACCEPTING_SOCKET_IS_NEX(accepting_socket))         processor_fixed_nex_failsafe(fixed, response_stream);
    else if (ACCEPTING_SOCKET_IS_TEXT(accepting_socket))        processor_fixed_text_failsafe(fixed, response_stream);
    else /* if (ACCEPTING_SOCKET_IS_GUPPY(accepting_socket)) */ processor_fixed_guppy_failsafe(fixed, response_stream);
}

struct Error *processor_print(unsigned char accepting_socket, struct ProcessorPrintContext *context,
    enum EntryStyle style, const char *resource, const char *format, ...)
{
    struct Error *error;
    va_list va;
    va_start(va, format);
    context->list_index = (style == ES_ENUMERATION) ? (context->list_index + 1) : 0;
    if (ACCEPTING_SOCKET_IS_HTTP(accepting_socket))             { PGOTO(processor_print_http(context, style, resource, format, va)); }
    else if (ACCEPTING_SOCKET_IS_HTTPS(accepting_socket))       { PGOTO(processor_print_http(context, style, resource, format, va)); }
    else if (ACCEPTING_SOCKET_IS_GOPHER(accepting_socket))      { PGOTO(processor_print_gopher(context, style, resource, format, va)); }
    else if (ACCEPTING_SOCKET_IS_FINGER(accepting_socket))      { PGOTO(processor_print_finger(context, style, resource, format, va)); }
    else if (ACCEPTING_SOCKET_IS_GEMINI(accepting_socket))      { PGOTO(processor_print_gemini(context, style, resource, format, va)); }
    else if (ACCEPTING_SOCKET_IS_SPARTAN(accepting_socket))     { PGOTO(processor_print_spartan(context, style, resource, format, va)); }
    else if (ACCEPTING_SOCKET_IS_NEX(accepting_socket))         { PGOTO(processor_print_nex(context, style, resource, format, va)); }
    else if (ACCEPTING_SOCKET_IS_TEXT(accepting_socket))        { PGOTO(processor_print_text(context, style, resource, format, va)); }
    else /* if (ACCEPTING_SOCKET_IS_GUPPY(accepting_socket)) */ { PGOTO(processor_print_guppy(context, style, resource, format, va)); }
    error = OK;
    failure:
    context->previous_style = style;
    va_end(va);
    return error;
}
