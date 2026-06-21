#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/language.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/ring.h"

#include <string.h>

#define CRLF "\r\n"
#define ENDLINE "\n" /* Specification doesn't say anything, but LF is extremely convenient for lien wrap */

struct ExError processor_process_text(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstantValue zero = ZERO_INIT;
    union Value method, resource, protocol;
    
    *response_stream = zero;
    response_stream->parts[0].p = "40 Invalid resource" CRLF;
    response_stream->parts[0].size = strlen(response_stream->parts[0].p);

    EXPRETF(ring_get(request_stream, &request->method, false, &method.m), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource.m), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol.m), EEF_CLOSE_LOG_DIE);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &method.c, &resource.c, &protocol.c));

    return EXOK;
}

struct ExError processor_fixed_text(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstantValue zero = ZERO_INIT;

    processor_fixed_text_failsafe(fixed, response_stream);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->size, false, 0, 0, &zero, &zero, &zero));

    return EXOK;
}

void processor_fixed_text_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream)
{
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    response_stream->p = "40 Maximum number of clients reached" CRLF;       break;
    case FR_MAX_MEMORY:                     response_stream->p = "40 Maximum memory usage reached" CRLF;            break;
    case FR_MAX_UTILIZATION:                response_stream->p = "40 Maximum processor utilization reached" CRLF;   break;
    case FR_UNKNOWN:                        response_stream->p = "40 Unknown error" CRLF;                           break;
    case FR_REQUEST_INVALID:                response_stream->p = "40 Parser error" CRLF;                            break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   response_stream->p = "40 Received too large chunk of data" CRLF;        break;
    case FR_MAX_REQUEST_HEADER_SIZE:        response_stream->p = "40 Actual header size is too large" CRLF;         break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       response_stream->p = "40 Promised content size is too large" CRLF;      break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    response_stream->p = "40 Request incomplete for too long" CRLF;         break;
    default:                                response_stream->p = "40 Unknown error" CRLF;                           break;
    }
    response_stream->size = strlen(response_stream->p);
}

struct Error *processor_print_text(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    switch (style)
    {
    case ES_INITIALIZE: /* Overload, different status code */
        PRET(string_copy_mem(context->one, STRING_STRLEN("20 text/plain" CRLF)));
        context->two->size = 0;
        break;

    case ES_INTERNAL_REFERENCE: /* Overload, different protocol */
        PRET(string_print_append(context->one, "=> text://" DOMAIN_NAME TEXT_PORT_STRING "/%s%s ", 
            resource, language_slash(context->language, context->requested_language, context->request->language)));
        PRET(string_vprint_append(context->one, format, va));
        PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));
        break;

    default: /* Inherit */
        PRET(processor_print_nex(context, style, resource, format, va));
        break;
    }
    return OK;
}
