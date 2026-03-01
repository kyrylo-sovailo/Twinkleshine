#ifndef RESPONSE_PRINTER_H
#define RESPONSE_PRINTER_H

#include "char_buffer.h"
#include "error.h"
#include "request_response.h"

/* Transforms response into buffer */
struct Error *print_response(const struct Response *response, struct CharBuffer *buffer) NODISCARD;

#endif
