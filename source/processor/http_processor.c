#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/macro.h"
#include "../../include/parser.h"
#include "../../include/ring.h"
#include "../../include/tables.h"
#include "../../include/time.h"

#include <string.h>
#include <time.h>

#define CRLF "\r\n"
#define ENDLINE "\n" /* Is there specification? */

/* Static pieces of text */
static const char http_template_error[] =
"HTTP/1.1 %s" CRLF
"Server: Twinkleshine" CRLF
"Cache-Control: no-cache, no-store, must-revalidate" CRLF
"Content-Type: text/html; charset=UTF-8" CRLF
"Content-Length: %u" CRLF
"Connection: %s" CRLF
"" CRLF;

static const char http_template_success[] = 
"HTTP/1.1 200 OK" CRLF
"Server: Twinkleshine" CRLF
"Cache-Control: no-cache, no-store, must-revalidate" CRLF
"Content-Type: text/html; charset=UTF-8" CRLF
"Content-Length: %u" CRLF
"Connection: %s" CRLF
"Date: %s, %02d %s %d %02d:%02d:%02d GMT" CRLF
"" CRLF;

static const char html_template_error_page[] =
"<p>%s</p>\n"
"<p>%s</p>\n";

static const char html_open[] =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <meta charset=\"utf-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
"    <title>My 8-bit soul</title>\n"
"</head>\n"
"<body>\n";

static const char html_close[] =
"</body>\n"
"</html>\n";

static const char txt_robots[] = 
"User-agent: *\n"
"Disallow: /\n";

/* Buffer for fixed responses (must be generated because I can't get content length in compile time without magic numbers) */
static struct CharBuffer g_http_fixed_max_clients = ZERO_INIT;
static struct CharBuffer g_http_fixed_max_memory = ZERO_INIT;
static struct CharBuffer g_http_fixed_max_utilization = ZERO_INIT;
static struct CharBuffer g_http_fixed_unknown = ZERO_INIT;
static struct CharBuffer g_http_fixed_request_invalid = ZERO_INIT;
static struct CharBuffer g_http_fixed_max_available_request_stream = ZERO_INIT;
static struct CharBuffer g_http_fixed_max_request_header_size = ZERO_INIT;
static struct CharBuffer g_http_fixed_max_request_content_size = ZERO_INIT;
static struct CharBuffer g_http_fixed_max_incomplete_request_time = ZERO_INIT;

static struct Error *processor_module_initialize_one(struct CharBuffer *buffer, const char *status, const char *explanation) NODISCARD;
static struct Error *processor_module_initialize_one(struct CharBuffer *buffer, const char *status, const char *explanation)
{
    const unsigned int content_size =
        sizeof(html_open) - 1 +
        (unsigned int)(sizeof(html_template_error_page) - 1 - 4 + strlen(status) + strlen(explanation)) + /* 4 because two %s sequences */
        sizeof(html_close) - 1;
    PRET(string_print(buffer, http_template_error, status, (unsigned int)content_size, "close"));
    PRET(string_append_mem(buffer, html_open, sizeof(html_open) - 1));
    PRET(string_print_append(buffer, html_template_error_page, status, explanation));
    PRET(string_append_mem(buffer, html_close, sizeof(html_close) - 1));
    return OK;
}

struct Error *processor_module_initialize_http(void)
{
    PRET(processor_module_initialize_one(&g_http_fixed_max_clients,
        "503 Service Unavailable", "Maximum number of clients reached"));
    PRET(processor_module_initialize_one(&g_http_fixed_max_memory,
        "503 Service Unavailable", "Maximum memory usage reached"));
    PRET(processor_module_initialize_one(&g_http_fixed_max_utilization,
        "503 Service Unavailable", "Maximum processor utilization reached"));
    PRET(processor_module_initialize_one(&g_http_fixed_unknown,
        "500 Internal Server Error", "Unknown error"));
    PRET(processor_module_initialize_one(&g_http_fixed_request_invalid,
        "400 Bad Request", "Parser error"));
    PRET(processor_module_initialize_one(&g_http_fixed_max_available_request_stream,
        "429 Too Many Requests", "Received too large chunk of data"));
    PRET(processor_module_initialize_one(&g_http_fixed_max_request_header_size,
        "431 Request Header Fields Too Large", "Actual header size is too large"));
    PRET(processor_module_initialize_one(&g_http_fixed_max_request_content_size,
        "413 Content Too Large", "Promised content size is too large"));
    PRET(processor_module_initialize_one(&g_http_fixed_max_incomplete_request_time,
        "408 Request Timeout", "Request incomplete for too long"));
    return OK;
}

void processor_module_finalize_http(void)
{
    string_finalize(&g_http_fixed_max_clients);
    string_finalize(&g_http_fixed_max_memory);
    string_finalize(&g_http_fixed_max_utilization);
    string_finalize(&g_http_fixed_unknown);
    string_finalize(&g_http_fixed_request_invalid);
    string_finalize(&g_http_fixed_max_available_request_stream);
    string_finalize(&g_http_fixed_max_request_header_size);
    string_finalize(&g_http_fixed_max_request_content_size);
    string_finalize(&g_http_fixed_max_incomplete_request_time);

    string_finalize(&g_internal_buffer_one);
    string_finalize(&g_internal_buffer_two);
}

struct Error *processor_print_http(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va)
{
    /* Prefix */
    if (context->previous_style == ES_ITEMIZE && style != ES_ITEMIZE) { PRET(string_append_mem(context->two, STRING_STRLEN("</ol>" ENDLINE))); }
    if (context->previous_style == ES_ENUMERATION && style != ES_ENUMERATION) { PRET(string_append_mem(context->two, STRING_STRLEN("</ul>" ENDLINE))); }
    context->previous_style = style;
    switch (style)
    {
    case ES_INITIALIZE:
    {
        PRET(string_copy_mem(context->two, STRING_STRLEN(html_open)));
        return OK;
    }
    case ES_FINALIZE:
    {
        const struct tm global_calender = time_to_calender(time(NULL), true);
        PRET(string_append_mem(context->two, STRING_STRLEN(html_close)));
        PRET(string_print(context->one, http_template_success, (unsigned int)context->two->size, context->request->keep_alive ? "keep-alive" : "close",
            days_xxx[global_calender.tm_wday], global_calender.tm_mday, months_xxx[global_calender.tm_mon], 1900 + global_calender.tm_year,
            global_calender.tm_hour, global_calender.tm_min, global_calender.tm_sec));
        return OK;
    }
    case ES_NORMAL:
    {
        PRET(string_append_mem(context->two, STRING_STRLEN("<p>")));
        break;
    }
    case ES_ITEMIZE:
    {
        if (context->previous_style != ES_ITEMIZE) { PRET(string_append_mem(context->two, STRING_STRLEN("<ul>" ENDLINE))); }
        PRET(string_append_mem(context->two, STRING_STRLEN("<li>")));
        break;
    }
    case ES_ENUMERATION:
    {
        if (context->previous_style != ES_ITEMIZE) { PRET(string_append_mem(context->two, STRING_STRLEN("<ol>" ENDLINE))); }
        PRET(string_append_mem(context->two, STRING_STRLEN("<li>")));
        break;
    }
    case ES_QUOTE: PRET(string_append_mem(context->two, STRING_STRLEN("<blockquote>" ENDLINE "<p>"))); break;
    case ES_LARGE: PRET(string_append_mem(context->two, STRING_STRLEN("<h3>"))); break;
    case ES_LARGER: PRET(string_append_mem(context->two, STRING_STRLEN("<h2>"))); break;
    case ES_LARGEST: PRET(string_append_mem(context->two, STRING_STRLEN("<h1>"))); break;
    case ES_HEADER: PRET(string_append_mem(context->two, STRING_STRLEN("<header>" ENDLINE "<h1>"))); break;
    case ES_INTERNAL_REFERENCE: PRET(string_print_append(context->two, "<p><a href=\"/%s\">", resource)); break;
    case ES_EXTERNAL_REFERENCE: PRET(string_print_append(context->two, "<p><a href=\"%s\">", resource)); break;
    }
    
    /* Text */
    PRET(string_vprint_append(context->two, format, va));

    /* Suffix */
    switch (style)
    {
    case ES_NORMAL: PRET(string_append_mem(context->two, STRING_STRLEN("</p>" ENDLINE))); break;
    case ES_ITEMIZE:
    case ES_ENUMERATION: PRET(string_append_mem(context->two, STRING_STRLEN("</li>" ENDLINE))); break;
    case ES_QUOTE: PRET(string_append_mem(context->two, STRING_STRLEN("</p>" ENDLINE "</blockquote>" ENDLINE))); break;
    case ES_LARGE: PRET(string_append_mem(context->two, STRING_STRLEN("</h3>" ENDLINE))); break;
    case ES_LARGER: PRET(string_append_mem(context->two, STRING_STRLEN("</h2>" ENDLINE))); break;
    case ES_LARGEST: PRET(string_append_mem(context->two, STRING_STRLEN("</h1>" ENDLINE))); break;
    case ES_HEADER: PRET(string_append_mem(context->two, STRING_STRLEN("</h1>" ENDLINE "</header>" ENDLINE))); break;
    default: PRET(string_append_mem(context->two, STRING_STRLEN("</p></a>" ENDLINE))); break; /* ES_INTERNAL_REFERENCE, ES_EXTERNAL_REFERENCE */
    }
    return OK;
}

struct ExError processor_process_http(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct Value *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Value zero = ZERO_INIT;
    struct Value method, resource, protocol;
    bool keep_alive;
    
    EXPRETF(ring_get(request_stream, &request->method, false, &method), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->protocol, false, &protocol), EEF_CLOSE_LOG_DIE);
    if (!value_compare_case_mem(&method, STRING_STRLEN("get")))
    {
        keep_alive = request->keep_alive;
        EXPRETF(string_copy_mem(&g_internal_buffer_two, html_open, sizeof(html_open) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print_append(&g_internal_buffer_two, html_template_error_page, "405 Method Not Allowed", "Invalid method"), EEF_CLOSE_LOG);
        EXPRETF(string_append_mem(&g_internal_buffer_two, html_close, sizeof(html_close) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print(&g_internal_buffer_one, http_template_error, "405 Method Not Allowed", (unsigned int)g_internal_buffer_two.size, keep_alive ? "keep-alive" : "close"), EEF_CLOSE_LOG);
    }
    else if (value_compare_case_mem(&resource, STRING_STRLEN("/robots.txt")))
    {
        const struct tm global_calender = time_to_calender(time(NULL), true);
        keep_alive = false;
        EXPRETF(string_copy_mem(&g_internal_buffer_two, txt_robots, sizeof(txt_robots) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print(&g_internal_buffer_one, http_template_success, (unsigned int)g_internal_buffer_two.size, keep_alive ? "keep-alive" : "close",
            days_xxx[global_calender.tm_wday], global_calender.tm_mday, months_xxx[global_calender.tm_mon], 1900 + global_calender.tm_year,
            global_calender.tm_hour, global_calender.tm_min, global_calender.tm_sec), EEF_CLOSE_LOG);
    }
    else
    {
        keep_alive = request->keep_alive;
        EXPRETF(string_copy_mem(&g_internal_buffer_two, STRING_STRLEN(html_open)), EEF_CLOSE_LOG);
        EXPRETF(string_print_append(&g_internal_buffer_two, html_template_error_page, "404 Not Found", "Invalid resource"), EEF_CLOSE_LOG);
        EXPRETF(string_append_mem(&g_internal_buffer_two, STRING_STRLEN(html_close)), EEF_CLOSE_LOG);
        EXPRETF(string_print(&g_internal_buffer_one, http_template_error, "404 Not Found", (unsigned int)g_internal_buffer_two.size, keep_alive ? "keep-alive" : "close"), EEF_CLOSE_LOG);
    }
    *response_stream = zero;
    response_stream->parts[0].p = g_internal_buffer_one.p;
    response_stream->parts[0].size = g_internal_buffer_one.size;
    response_stream->parts[1].p = g_internal_buffer_two.p;
    response_stream->parts[1].size = g_internal_buffer_two.size;

    EXPRET(processor_construct_response(response, response_queue,
        value_size(response_stream), keep_alive, 0, 0, &method, &resource, &protocol));

    return EXOK;
}

struct ExError processor_fixed_http(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct Value *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Value zero = ZERO_INIT;

    processor_fixed_http_failsafe(fixed, response_stream);
    EXPRET(processor_construct_response(response, response_queue,
        response_stream->parts[0].size, false, 0, 0, &zero, &zero, &zero));

    return EXOK;
}

void processor_fixed_http_failsafe(enum FixedResponse fixed, struct Value *response_stream)
{
    const struct Value zero = ZERO_INIT;
    struct CharBuffer *fixed_string;
    switch (fixed)
    {
    case FR_MAX_CLIENTS:                    fixed_string = &g_http_fixed_max_clients;                   break;
    case FR_MAX_MEMORY:                     fixed_string = &g_http_fixed_max_memory;                    break;
    case FR_MAX_UTILIZATION:                fixed_string = &g_http_fixed_max_utilization;               break;
    case FR_UNKNOWN:                        fixed_string = &g_http_fixed_unknown;                       break;
    case FR_REQUEST_INVALID:                fixed_string = &g_http_fixed_request_invalid;               break;
    case FR_MAX_AVAILABLE_REQUEST_STREAM:   fixed_string = &g_http_fixed_max_available_request_stream;  break;
    case FR_MAX_REQUEST_HEADER_SIZE:        fixed_string = &g_http_fixed_max_request_header_size;       break;
    case FR_MAX_REQUEST_CONTENT_SIZE:       fixed_string = &g_http_fixed_max_request_content_size;      break;
    case FR_MAX_INCOMPLETE_REQUEST_TIME:    fixed_string = &g_http_fixed_max_incomplete_request_time;   break;
    default:                                fixed_string = &g_http_fixed_unknown;                       break;
    }
    *response_stream = zero;
    response_stream->parts[0].p = fixed_string->p;
    response_stream->parts[0].size = fixed_string->size;
}
