#include "../include/request_processor.h"
#include "../include/string.h"
#include "../include/tables.h"

#include <string.h>
#include <time.h>

/* Static responses that the server doesn't waste any time forming */
#define COMMON_STATIC_HTTP_RESPONSE() \
"Server: Twinkleshine\r\n" \
"Content-Length: 0\r\n" \
"Connection: close\r\n" \
"\r\n"

static const char http_static_max_clients[] = "HTTP/1.1 503 Service Unavailable\r\n" COMMON_STATIC_HTTP_RESPONSE();
static const size_t http_static_max_clients_length = sizeof(http_static_max_clients);

static const char http_static_max_memory[] = "HTTP/1.1 503 Service Unavailable\r\n" COMMON_STATIC_HTTP_RESPONSE(); /* TODO: merge with http_static_max_clients */
static const size_t http_static_max_memory_length = sizeof(http_static_max_memory);

static const char http_static_max_utilization[] = "HTTP/1.1 503 Service Unavailable\r\n" COMMON_STATIC_HTTP_RESPONSE(); /* TODO: merge with http_static_max_clients */
static const size_t http_static_max_utilization_length = sizeof(http_static_max_utilization);

static const char http_static_unknown[] = "HTTP/1.1 500 Internal Server Error\r\n" COMMON_STATIC_HTTP_RESPONSE();
static const size_t http_static_unknown_length = sizeof(http_static_unknown);

static const char http_static_too_fast[] = "HTTP/1.1 200 OK\r\n" COMMON_STATIC_HTTP_RESPONSE();
static const size_t http_static_too_fast_length = sizeof(http_static_too_fast);

static const char http_static_too_large[] = "HTTP/1.1 431 Request Header Fields Too Large\r\n" COMMON_STATIC_HTTP_RESPONSE();
static const size_t http_static_too_large_length = sizeof(http_static_too_large);

static const char http_static_incomplete_input[] = "HTTP/1.1 408 Request Timeout\r\n" COMMON_STATIC_HTTP_RESPONSE();
static const size_t http_static_incomplete_input_length = sizeof(http_static_incomplete_input);

static const char http_static_parsing_failed[] = "HTTP/1.1 400 Bad Request\r\n" COMMON_STATIC_HTTP_RESPONSE();
static const size_t http_static_parsing_failed_length = sizeof(http_static_parsing_failed);

static const char http_static_not_found[] = "HTTP/1.1 404 Not Found\r\n" COMMON_STATIC_HTTP_RESPONSE();
static const size_t http_static_not_found_length = sizeof(http_static_not_found);

/* Actual content generation */
static const char http_prototype_html[] = 
"HTTP/1.1 200 OK\r\n"
"Server: Twinkleshine\r\n"
"Content-Length: %u\r\n"
"Connection: %s\r\n"
"Date: %s, %02d %s %d %02d:%02d:%02d GMT\r\n"
"Content-Type: text/html; charset=UTF-8\r\n"
"\r\n";

static const char html_static_index_page[] =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"<title>Page Title</title>\n"
"</head>\n"
"<body>\n"
"<h1>This is a Heading</h1>\n"
"<p>This is a paragraph.</p>\n"
"</body>\n"
"</html>\n";

static struct CharBuffer buffer = { 0 };

struct Error *request_processor_process(const struct Ring *input, const struct Request *request, struct ValuePart *response)
{
    struct Value resource_value;

    PRET(ring_get(input, &request->resource, false, &resource_value));
    if (value_compare_mem(&resource_value, "/", strlen("/")))
    {
        struct Error *error;
        time_t global_time;
        struct tm *p_global_calender, global_calender;

        /* Get time */
        global_time = time(NULL);
        p_global_calender = gmtime(&global_time); /* TODO: not thread-safe btw */
        if (p_global_calender != NULL) global_calender = *p_global_calender;

        /*
        Leaving full page in cache is close to impossible
        because the size of HTTP header (required for correct HTML print)
        and the size of HTML content (required for HTTP header)
        depend on once another and change constantly
        right now it was easy because the content is static,
        but it is not super fast or extendable
        */

        /* Print HTTP header */
        error = string_printf(&buffer, http_prototype_html,
            (unsigned int)sizeof(html_static_index_page),
            request->keep_alive ? "keep-alive" : "close",
            days_xxx[global_calender.tm_wday], global_calender.tm_mday, months_xxx[global_calender.tm_mon], 1900 + global_calender.tm_year,
            global_calender.tm_hour, global_calender.tm_min, global_calender.tm_sec
        );
        PRET(error);

        /* Print HTML content */
        error = string_printf_end(&buffer, html_static_index_page);
        PRET(error);

        response->p = buffer.p;
        response->size = buffer.size;
    }
    else
    {
        response->p = (char*)http_static_not_found; /* TODO: solve the problem with constant and not-constant value parts */
        response->size = http_static_not_found_length;
    }
    return OK;
}

void request_processor_error(enum ErrorType error, struct ValuePart *response)
{
    switch (error)
    {
    case ERR_MAX_CLIENTS:
        response->p = (char*)http_static_max_clients;
        response->size = http_static_max_clients_length;
        break;
        
    case ERR_MAX_MEMORY:
        response->p = (char*)http_static_max_memory;
        response->size = http_static_max_memory_length;
        break;

    case ERR_MAX_UTILIZATION:
        response->p = (char*)http_static_max_utilization;
        response->size = http_static_max_utilization_length;
        break;

    case ERR_UNKNOWN:
        response->p = (char*)http_static_unknown;
        response->size = http_static_unknown_length;
        break;

    case ERR_TOO_FAST:
        response->p = (char*)http_static_too_fast;
        response->size = http_static_too_fast_length;
        break;

    case ERR_TOO_LARGE:
        response->p = (char*)http_static_too_large;
        response->size = http_static_too_large_length;
        break;

    case ERR_INCOMPLETE_INPUT:
        response->p = (char*)http_static_incomplete_input;
        response->size = http_static_incomplete_input_length;
        break;

    default: /* ERR_PARSING_FAILED */
        response->p = (char*)http_static_parsing_failed;
        response->size = http_static_parsing_failed_length;
        break;
    }
}

void request_processor_free()
{
    /* Do nothing */
}