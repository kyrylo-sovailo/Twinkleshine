#include "../include/processor.h"
#include "../commonlib/include/error.h"
#include "../commonlib/include/string.h"
#include "../include/parser.h"
#include "../include/ring.h"
#include "../include/tables.h"

#include <stdbool.h>
#include <string.h>
#include <time.h>

/* Static pieces of text */
static const char http_template_error[] =
"HTTP/1.1 %s\r\n"
"Server: Twinkleshine\r\n"
"Content-Type: text/html; charset=UTF-8\r\n"
"Content-Length: %u\r\n"
"Connection: %s\r\n"
"\r\n";

static const char http_template_success[] = 
"HTTP/1.1 200 OK\r\n"
"Server: Twinkleshine\r\n"
"Content-Type: text/html; charset=UTF-8\r\n"
"Content-Length: %u\r\n"
"Connection: %s\r\n"
"Date: %s, %02d %s %d %02d:%02d:%02d GMT\r\n"
"\r\n";

static const char html_template_error_page[] =
"<p>%s</p>\n"
"<p>%s</p>\n";

static const char html_static_index_page[] =
"<header>\n"
"    <h1>This is Kyrylo's website.</h1>\n"
"</header>\n"
"<p>It is displaying correctly.</p>\n"
"<p>The style is not a bug. It is a feature.</p>\n"
"<p>Served to you by the <a href=\"https://github.com/kyrylo-sovailo/Twinkleshine\">Twinkleshine server</a>.</p>\n";

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

/* Buffer for normal response */
static struct CharBuffer g_header_buffer = ZERO_INIT, g_content_buffer = ZERO_INIT;

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

static void processor_set_locations_one(const struct ValueLocation *request_location, struct ValueLocation *response_location, size_t *size)
{
    response_location->offset = *size;
    response_location->size = request_location->size;
    *size += request_location->size;
}

static void processor_set_locations(const struct Request *request, struct Response *response)
{
    size_t size = 0;
    processor_set_locations_one(&request->method, &response->method, &size);
    processor_set_locations_one(&request->protocol, &response->protocol, &size);
    processor_set_locations_one(&request->resource, &response->resource, &size);
    response->size = size;
}

static struct Error *processor_push_metadata_one(const struct ValueLocation *request_location, const struct Ring *request_stream, struct Ring *response_queue) NODISCARD;
static struct Error *processor_push_metadata_one(const struct ValueLocation *request_location, const struct Ring *request_stream, struct Ring *response_queue)
{
    struct Value value;
    unsigned char i;
    
    PRET(ring_get(request_stream, request_location, false, &value));
    for (i = 0; i < VALUE_PARTS; i++)
    {
        PRET(ring_push_write(response_queue, request_location->size, (const char*)&value.parts[i]));
    }
    return OK;
}

static struct Error *processor_push_metadata(const struct Request *request, const struct Ring *request_stream, struct Ring *response_queue) NODISCARD;
static struct Error *processor_push_metadata(const struct Request *request, const struct Ring *request_stream, struct Ring *response_queue)
{
    PRET(processor_push_metadata_one(&request->method, request_stream, response_queue));
    PRET(processor_push_metadata_one(&request->protocol, request_stream, response_queue));
    PRET(processor_push_metadata_one(&request->resource, request_stream, response_queue));
    return OK;
}

struct Error *processor_module_initialize(void)
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

void processor_module_finalize(void)
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

    string_finalize(&g_header_buffer);
    string_finalize(&g_content_buffer);
}

struct ExError processor_process(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstValue zero = ZERO_INIT;
    const bool keep_alive = request->keep_alive;
    struct Response local_response;
    struct Value method, resource;

    processor_set_locations(request, &local_response);
    EXPRETF(ring_get(request_stream, &request->method, false, &method), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource), EEF_CLOSE_LOG_DIE);

    /* Create content and header */
    if (!value_compare_case_mem(&method, "get", strlen("get")))
    {
        EXPRETF(string_copy_mem(&g_content_buffer, html_open, sizeof(html_open) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print_append(&g_content_buffer, html_template_error_page, "405 Method Not Allowed", "Invalid method"), EEF_CLOSE_LOG);
        EXPRETF(string_append_mem(&g_content_buffer, html_close, sizeof(html_close) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print(&g_header_buffer, http_template_error, "405 Method Not Allowed", (unsigned int)g_content_buffer.size, keep_alive ? "keep-alive" : "close"), EEF_CLOSE_LOG);
    }
    else if (value_compare_case_mem(&resource, "/", strlen("/")))
    {
        time_t global_time;
        struct tm *p_global_calender, global_calender = ZERO_INIT;
        EXPRETF(string_copy_mem(&g_content_buffer, html_open, sizeof(html_open) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_append_mem(&g_content_buffer, html_static_index_page, sizeof(html_static_index_page) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_append_mem(&g_content_buffer, html_close, sizeof(html_close) - 1), EEF_CLOSE_LOG);
        global_time = time(NULL);
        p_global_calender = gmtime(&global_time); /* TODO: not thread-safe btw */
        if (p_global_calender != NULL) global_calender = *p_global_calender;
        EXPRETF(string_print(&g_header_buffer, http_template_success, (unsigned int)g_content_buffer.size, keep_alive ? "keep-alive" : "close",
            days_xxx[global_calender.tm_wday], global_calender.tm_mday, months_xxx[global_calender.tm_mon], 1900 + global_calender.tm_year,
            global_calender.tm_hour, global_calender.tm_min, global_calender.tm_sec), EEF_CLOSE_LOG);
    }
    else
    {
        EXPRETF(string_copy_mem(&g_content_buffer, html_open, sizeof(html_open) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print_append(&g_content_buffer, html_template_error_page, "404 Not Found", "Invalid resource"), EEF_CLOSE_LOG);
        EXPRETF(string_append_mem(&g_content_buffer, html_close, sizeof(html_close) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print(&g_header_buffer, http_template_error, "404 Not Found", (unsigned int)g_content_buffer.size, keep_alive ? "keep-alive" : "close"), EEF_CLOSE_LOG);
    }

    /* Response */
    local_response.keep_alive = keep_alive;
    local_response.stream_size = g_content_buffer.size;

    /* Metadata */
    EXPRETF(ring_reserve(response_queue, response_queue->size + (response == NULL ? sizeof(struct Response) : 0) + local_response.size), EEF_CLOSE_LOG);
    if (response == NULL) { EXPRETF(ring_push_write(response_queue, sizeof(struct Response), (char*)&local_response), EEF_CLOSE_LOG_DIE); }
    else *response = local_response;
    EXPRETF(processor_push_metadata(request, request_stream, response_queue), EEF_CLOSE_LOG_DIE);

    /* Data */
    *response_stream = zero;
    response_stream->parts[0].p = g_header_buffer.p;
    response_stream->parts[0].size = g_header_buffer.size;
    response_stream->parts[1].p = g_content_buffer.p;
    response_stream->parts[1].size = g_content_buffer.size;

    return EXOK;
}

void processor_fixed(enum FixedResponse fixed, struct ConstValue *response)
{
    const struct ConstValue zero = ZERO_INIT;
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
    *response = zero;
    response->parts[0].p = fixed_string->p;
    response->parts[0].size = fixed_string->size;
}

void processor_free(void)
{
    /* Do nothing */
}
