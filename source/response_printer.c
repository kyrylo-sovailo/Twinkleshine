#include "../include/response_printer.h"

#include <time.h>

struct Error *print_response(const struct Response *response, struct CharBuffer *buffer)
{
    const char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const char *days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    struct Error *error;
    time_t global_time;
    struct tm *p_global_calender, global_calender;

    /* Get time */
    global_time = time(NULL);
    p_global_calender = gmtime(&global_time); /* TODO: not thread-safe btw */
    if (p_global_calender != NULL) global_calender = *p_global_calender;

    /* Print */
    error = string_printf_end(buffer,
        "HTTP/1.1 200 OK\r\n"
        "Date: %s, %02d %s %d %02d:%02d:%02d GMT\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %u\r\n"
        "Server: Twinkleshine\r\n"
        "Connection: %s\r\n"
        "\r\n"
        "%p",
        days[global_calender.tm_wday], global_calender.tm_mday, months[global_calender.tm_mon], 1900 + global_calender.tm_year,
        global_calender.tm_hour, global_calender.tm_min, global_calender.tm_sec,
        (unsigned int)response->content.size,
        response->keep_alive ? "keep-alive" : "close",
        (void*)&response->content /* Custom printing rule for CharBuffer */
    );
    PRET(error);

    return OK;
}