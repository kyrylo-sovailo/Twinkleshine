#include "../include/request_processor.h"

#include <string.h>

static const char *hello_world =
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

static size_t hello_world_length;
static bool initialized = false;

static void initialize()
{
    hello_world_length = strlen(hello_world);
}

struct Error *process_request(const struct Request *request, struct Response *response)
{
    if (!initialized) { initialize(); initialized = true; }
    (void)request; /* Ignore actual request */
    response->success = true;
    response->keep_alive = request->keep_alive;
    PRET(string_copy_mem(&response->content, hello_world, hello_world_length));
    return OK;
}