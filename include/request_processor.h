#ifndef REQUEST_PROCESSOR_H
#define REQUEST_PROCESSOR_H

#include "error.h"
#include "request_response.h"

/* Process request and setup response */
struct Error *process_request(const struct Request *request, struct Response *response) NODISCARD;

#endif
