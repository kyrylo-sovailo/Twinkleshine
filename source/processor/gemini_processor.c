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
#define ENDLINE "\r\n" /* Specification says CR is optional */

struct ExError processor_process_gemini(const struct Request *request, const struct Ring *request_stream,
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

struct ExError processor_fixed_gemini(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstantValue zero = ZERO_INIT;

    processor_fixed_gemini_failsafe(fixed, response_stream);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->size, false, 0, 0, &zero, &zero, &zero));

    return EXOK;
}

void processor_fixed_gemini_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream)
{
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    response_stream->p = "41 Maximum number of clients reached" CRLF;       break;
    case FR_MAX_MEMORY:                     response_stream->p = "41 Maximum memory usage reached" CRLF;            break;
    case FR_MAX_UTILIZATION:                response_stream->p = "41 Maximum processor utilization reached" CRLF;   break;
    case FR_UNKNOWN:                        response_stream->p = "42 Unknown error" CRLF;                           break;
    case FR_REQUEST_INVALID:                response_stream->p = "59 Parser error" CRLF;                            break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   response_stream->p = "59 Received too large chunk of data" CRLF;        break;
    case FR_MAX_REQUEST_HEADER_SIZE:        response_stream->p = "59 Actual header size is too large" CRLF;         break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       response_stream->p = "59 Promised content size is too large" CRLF;      break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    response_stream->p = "59 Request incomplete for too long" CRLF;         break;
    default:                                response_stream->p = "42 Unknown error" CRLF;                           break;
    }
    response_stream->size = strlen(response_stream->p);
}

struct Error *processor_print_gemini(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    /* Prefix */
    switch (style)
    {
    case ES_INITIALIZE: PRET(string_copy_mem(context->one, STRING_STRLEN("20 text/gemini" CRLF))); context->two->size = 0; return OK;
    case ES_FINALIZE: return OK;
    case ES_NORMAL:
    case ES_PARAGRAPH: PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE))); break;
    case ES_ITEMIZE: PRET(string_append_mem(context->one, STRING_STRLEN("* "))); break;
    case ES_ENUMERATION: PRET(string_print_append(context->one, "%u. ", context->list_index)); break;
    case ES_QUOTE: PRET(string_append_mem(context->one, STRING_STRLEN("> "))); break;
    case ES_LARGE: PRET(string_append_mem(context->one, STRING_STRLEN("### "))); break;
    case ES_LARGER: PRET(string_append_mem(context->one, STRING_STRLEN("## "))); break;
    case ES_LARGEST:
    case ES_HEADER: PRET(string_append_mem(context->one, STRING_STRLEN("# "))); break;
    case ES_INTERNAL_REFERENCE: PRET(string_print_append(context->one, "=> gemini://" DOMAIN_NAME GEMINI_PORT_STRING "/%s%s ", resource, language_question(context->reference_language))); break;
    default: PRET(string_print_append(context->one, "=> %s ", resource)); break; /* ES_EXTERNAL_REFERENCE */
    }
    
    /* Text */
    PRET(string_vprint_append(context->one, format, va));

    /* Suffix */
    PRET(string_append_mem(context->one, STRING_STRLEN(ENDLINE)));

    return OK;
}
