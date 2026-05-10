#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "../commonlib/include/macro.h"
#include "../include/extended_error.h"
#include "../include/value.h"

struct Error;
struct Request;
struct Response;
struct Ring;

/* Identifies the fixed message that should be generated fast (does not interfere with ErrorAction) */
enum FixedResponse
{
    FR_MAX_CLIENTS = 0 << 8,        /* 2.1 */
    FR_MAX_MEMORY = 1 << 8,         /* 2.2 */
    FR_MAX_UTILIZATION,             /* 2.3 */
    FR_UNKNOWN,                     /* 4.1 */
    FR_REQUEST_INVALID,             /* 4.1 */
    FR_MAX_AVAILABLE_REQUEST_STREAM,/* 4.2 */
    FR_MAX_REQUEST_HEADER_SIZE,     /* 4.3 */
    FR_MAX_REQUEST_CONTENT_SIZE,    /* 4.3 */
    FR_MAX_INCOMPLETE_REQUEST_TIME  /* 4.4 */
};

/* Important meta-information of a response, which doesn't include the response itself */
struct Response
{
    size_t stream_size;             /* Total response size */
    size_t size;                    /* Total metadata size, not including Response */
    struct ValueLocation method;    /* Requested method */
    struct ValueLocation resource;  /* Requested resource */
    struct ValueLocation protocol;  /* Protocol version */
    bool keep_alive;                /* Keep connection alive */
};

/* Initialize the module */
struct Error *processor_module_initialize(void);

/* Finalize the module */
void processor_module_finalize(void);

/* Processes request and creates response (if response is NULL, response is pushed to response_queue) */
struct ExError processor_process(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;

/* Creates fixed response fast */
void processor_fixed(enum FixedResponse fixed, struct ConstValue *response);

/* Frees the response once it is no longer needed */
void processor_free(void);

#endif
