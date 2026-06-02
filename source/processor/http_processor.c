#include "../../include/processor.h"
#include "../../commonlib/include/error.h"
#include "../../commonlib/include/string.h"
#include "../../include/parser.h"
#include "../../include/random.h"
#include "../../include/ring.h"
#include "../../include/tables.h"
#include "../../include/time.h"

#include <stdbool.h>
#include <string.h>
#include <time.h>

/* Static pieces of text */
static const char http_template_error[] =
"HTTP/1.1 %s\r\n"
"Server: Twinkleshine\r\n"
"Cache-Control: no-cache, no-store, must-revalidate\r\n"
"Content-Type: text/html; charset=UTF-8\r\n"
"Content-Length: %u\r\n"
"Connection: %s\r\n"
"\r\n";

static const char http_template_success[] = 
"HTTP/1.1 200 OK\r\n"
"Server: Twinkleshine\r\n"
"Cache-Control: no-cache, no-store, must-revalidate\r\n"
"Content-Type: text/html; charset=UTF-8\r\n"
"Content-Length: %u\r\n"
"Connection: %s\r\n"
"Date: %s, %02d %s %d %02d:%02d:%02d GMT\r\n"
"\r\n";

static const char html_template_error_page[] =
"<p>%s</p>\n"
"<p>%s</p>\n";

static const char html_template_index_page[] =
"<header>\n"
"    <h1>This is Kyrylo's website.</h1>\n"
"</header>\n"
"<p>It is displaying correctly.</p>\n"
"<p>The style is not a bug. It is a feature.</p>\n"
"<p>Served to you by the <a href=\"https://github.com/kyrylo-sovailo/Twinkleshine\">Twinkleshine server</a>, fully written in C by yours truly.</p>\n"
"<p>Fun fact: %s</p>\n";

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

static void processor_set_locations_one(const struct ValueLocation *request_location, struct ValueLocation *response_location, size_t *size)
{
    response_location->offset = *size;
    response_location->size = request_location->size;
    *size += request_location->size;
}

static void processor_set_locations(const struct Request *request, struct Response *response)
{
    response->size = 0;
    processor_set_locations_one(&request->method, &response->method, &response->size);
    processor_set_locations_one(&request->protocol, &response->protocol, &response->size);
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
    PRET(processor_push_metadata_one(&request->method, request_stream, response_queue));
    PRET(processor_push_metadata_one(&request->protocol, request_stream, response_queue));
    PRET(processor_push_metadata_one(&request->resource, request_stream, response_queue));
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

struct ExError processor_process_http(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct ConstValue zero = ZERO_INIT;
    struct Value method, resource;
    bool keep_alive = request->keep_alive;
    
    /* Actual logic */
    EXPRETF(ring_get(request_stream, &request->method, false, &method), EEF_CLOSE_LOG_DIE);
    EXPRETF(ring_get(request_stream, &request->resource, false, &resource), EEF_CLOSE_LOG_DIE);
    if (!value_compare_case_mem(&method, "get", strlen("get")))
    {
        EXPRETF(string_copy_mem(&g_internal_buffer_two, html_open, sizeof(html_open) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print_append(&g_internal_buffer_two, html_template_error_page, "405 Method Not Allowed", "Invalid method"), EEF_CLOSE_LOG);
        EXPRETF(string_append_mem(&g_internal_buffer_two, html_close, sizeof(html_close) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print(&g_internal_buffer_one, http_template_error, "405 Method Not Allowed", (unsigned int)g_internal_buffer_two.size, keep_alive ? "keep-alive" : "close"), EEF_CLOSE_LOG);
    }
    else if (value_compare_case_mem(&resource, "/", strlen("/")))
    {
        const struct tm global_calender = time_to_calender(time(NULL), true);
        EXPRETF(string_copy_mem(&g_internal_buffer_two, html_open, sizeof(html_open) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print_append(&g_internal_buffer_two, html_template_index_page, fun_facts[random_rand(sizeof(fun_facts)/sizeof(*fun_facts))]), EEF_CLOSE_LOG);
        EXPRETF(string_append_mem(&g_internal_buffer_two, html_close, sizeof(html_close) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print(&g_internal_buffer_one, http_template_success, (unsigned int)g_internal_buffer_two.size, keep_alive ? "keep-alive" : "close",
            days_xxx[global_calender.tm_wday], global_calender.tm_mday, months_xxx[global_calender.tm_mon], 1900 + global_calender.tm_year,
            global_calender.tm_hour, global_calender.tm_min, global_calender.tm_sec), EEF_CLOSE_LOG);
    }
    else if (value_compare_case_mem(&resource, "/robots.txt", strlen("/robots.txt")))
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
        EXPRETF(string_copy_mem(&g_internal_buffer_two, html_open, sizeof(html_open) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print_append(&g_internal_buffer_two, html_template_error_page, "404 Not Found", "Invalid resource"), EEF_CLOSE_LOG);
        EXPRETF(string_append_mem(&g_internal_buffer_two, html_close, sizeof(html_close) - 1), EEF_CLOSE_LOG);
        EXPRETF(string_print(&g_internal_buffer_one, http_template_error, "404 Not Found", (unsigned int)g_internal_buffer_two.size, keep_alive ? "keep-alive" : "close"), EEF_CLOSE_LOG);
    }
    *response_stream = zero;
    response_stream->parts[0].p = g_internal_buffer_one.p;
    response_stream->parts[0].size = g_internal_buffer_one.size;
    response_stream->parts[1].p = g_internal_buffer_two.p;
    response_stream->parts[1].size = g_internal_buffer_two.size;

    /* Response */
    processor_set_locations(request, response);
    response->keep_alive = keep_alive;
    response->stream_size = g_internal_buffer_one.size + g_internal_buffer_two.size;

    /* Metadata */
    EXPRETF(ring_reserve(response_queue, response_queue->size + sizeof(struct Response) + response->size), EEF_CLOSE_LOG);
    EXPRETF(ring_push_write(response_queue, sizeof(struct Response), (const char*)response), EEF_CLOSE_LOG_DIE);
    EXPRETF(processor_push_metadata(request, request_stream, response_queue), EEF_CLOSE_LOG_DIE);

    return EXOK;
}

struct ExError processor_fixed_http(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    const struct Response zero = ZERO_INIT;

    /* Actual logic */
    processor_fixed_http_failsafe(fixed, response_stream);

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

void processor_fixed_http_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream)
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
    *response_stream = zero;
    response_stream->parts[0].p = fixed_string->p;
    response_stream->parts[0].size = fixed_string->size;
}
