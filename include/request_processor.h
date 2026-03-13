#ifndef REQUEST_PROCESSOR_H
#define REQUEST_PROCESSOR_H

#include "error.h"
#include "request.h"
#include "ring.h"
#include "value.h"

/* Identifies the error that should be sent, if any error is sent at all */
#define ERROR_TYPE_BITS 8
enum ErrorType
{
    ERR_MAX_CLIENTS,        /* 2.1 */
    ERR_MAX_MEMORY,         /* 2.2 */
    ERR_MAX_UTILIZATION,    /* 2.3 */

    ERR_UNKNOWN,            /* 4.1 */
    ERR_TOO_FAST,           /* 4.2 */
    ERR_TOO_LARGE,          /* 4.3 */
    ERR_INCOMPLETE_INPUT,   /* 4.4 */

    ERR_PARSING_FAILED
};

/* Processes request and creates response */
struct Error *request_processor_process(const struct Ring *input, const struct Request *request, struct ValuePart *response) NODISCARD;

/* Creates response, except that we don't have the request, only an error */
void request_processor_error(enum ErrorType error, struct ValuePart *response);

/* Frees the response once it is no longer needed */
void request_processor_free(void);

#endif
