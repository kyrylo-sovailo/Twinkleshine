#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/macro.h"
#include "../../commonlib/include/string.h"
#include "../../include/constants.h"
#include "../../include/language.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/ring.h"

#include <string.h>

#define FAKE "\tfake\t(NULL)\t0\r\n"
#define ENDLINE "\r\n" /* Specification says CRLF */
#define FAKE_ENDLINE FAKE ENDLINE

struct ExError processor_process_gopher(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstantValue zero = ZERO_INIT;
    union Value method, resource, protocol;
    
    *response_stream = zero;
    response_stream->parts[0].p = "3Invalid resource" FAKE_ENDLINE;
    response_stream->parts[0].size = strlen(response_stream->parts[0].p);

    EXPRETF(ring_get(request_stream, &request->method, false, &method.m), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource.m), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol.m), EEF_CLOSE_LOG_DIE);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &method.c, &resource.c, &protocol.c));

    return EXOK;
}

struct ExError processor_fixed_gopher(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstantValue zero = ZERO_INIT;

    processor_fixed_gopher_failsafe(fixed, response_stream);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->size, false, 0, 0, &zero, &zero, &zero));

    return EXOK;
}

void processor_fixed_gopher_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream)
{
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    response_stream->p = "3Maximum number of clients reached" FAKE_ENDLINE;     break;
    case FR_MAX_MEMORY:                     response_stream->p = "3Maximum memory usage reached" FAKE_ENDLINE;          break;
    case FR_MAX_UTILIZATION:                response_stream->p = "3Maximum processor utilization reached" FAKE_ENDLINE; break;
    case FR_UNKNOWN:                        response_stream->p = "3Unknown error" FAKE_ENDLINE;                         break;
    case FR_REQUEST_INVALID:                response_stream->p = "3Parser error" FAKE_ENDLINE;                          break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   response_stream->p = "3Received too large chunk of data" FAKE_ENDLINE;      break;
    case FR_MAX_REQUEST_HEADER_SIZE:        response_stream->p = "3Actual header size is too large" FAKE_ENDLINE;       break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       response_stream->p = "3Promised content size is too large" FAKE_ENDLINE;    break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    response_stream->p = "3Request incomplete for too long" FAKE_ENDLINE;       break;
    default:                                response_stream->p = "3Unknown error" FAKE_ENDLINE;                         break;
    }
    response_stream->size = strlen(response_stream->p);
}

struct Error *processor_print_gopher(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    /* Pre-prefix */
    size_t old_size;
    if (processor_generic_paragraph(style) || processor_generic_paragraph(context->previous_style))
    {
        PRET(string_append_mem(context->one, STRING_STRLEN("i" FAKE_ENDLINE)));
    }
    old_size = context->one->size;
    
    /* Prefix */
    switch (style)
    {
    case ES_INITIALIZE: context->one->size = 0; context->two->size = 0; return OK;
    case ES_FINALIZE: return OK;
    case ES_ITEMIZE: PRET(string_append_mem(context->one, STRING_STRLEN("i - "))); break;
    case ES_ENUMERATION: PRET(string_print_append(context->one, "i %u. ", context->list_index)); break;
    case ES_QUOTE: PRET(string_append_mem(context->one, STRING_STRLEN("i > "))); break;
    case ES_LARGEST:
    case ES_HEADER: PRET(string_append_mem(context->one, STRING_STRLEN("iO "))); break;
    case ES_INTERNAL_REFERENCE: PRET(string_print_append(context->one, "1")); break;
    default: PRET(string_append_mem(context->one, STRING_STRLEN("i"))); break;
    }
    
    /* Text */
    PRET(string_vprint_append(context->one, format, va));

    /* Suffix */
    switch (style)
    {
    case ES_LARGE:
    case ES_LARGER:
        PRET(string_append_mem(context->one, STRING_STRLEN(FAKE_ENDLINE)));
        processor_generic_upper_case(context->one, old_size, sizeof("i")-1, sizeof(FAKE_ENDLINE)-1);
        break;
    case ES_LARGEST:
    case ES_HEADER:
        PRET(string_append_mem(context->one, STRING_STRLEN(FAKE_ENDLINE)));
        processor_generic_upper_case(context->one, old_size, sizeof("i")-1, sizeof(FAKE_ENDLINE)-1);
        PRET(processor_generic_box(context->one, old_size, sizeof("i")-1, sizeof(FAKE_ENDLINE)-1, 'O', '0'));
        break;
    case ES_INTERNAL_REFERENCE:
        PRET(string_print_append(context->one, "\t%s%s\t" DOMAIN_NAME "\t" STRINGIZE(GOPHER_PORT) ENDLINE, resource, language_slash(context->reference_language)));
        break;
    case ES_EXTERNAL_REFERENCE:
        PRET(string_print_append(context->one, ": %s" FAKE_ENDLINE, resource));
        break;
    default:
        PRET(string_append_mem(context->one, STRING_STRLEN(FAKE_ENDLINE)));
        break;
    }
    return OK;
}
