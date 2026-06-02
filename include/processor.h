#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "../commonlib/include/macro.h"
#include "../include/extended_error.h"
#include "../include/value.h"

struct Error;
struct Request;
struct Response;
struct Ring;

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

/* Processes request and creates response (response + metadata + stream) */
struct ExError processor_process(int type, const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;

/* Creates fixed response (response + metadata + stream) */
struct ExError processor_fixed(int type, enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;

/* Creates fixed response very fast and without errors */
void processor_fixed_failsafe(int type, enum FixedResponse fixed, struct ConstValue *response_stream);

/* Frees the response once it is no longer needed */
void processor_free(void);

/* Internal */
struct Error *processor_module_initialize_http(void);
void processor_module_finalize_http(void);
struct ExError processor_process_http(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_process_gopher(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_process_finger(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_process_gemini(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_fixed_http(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_fixed_gopher(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_fixed_finger(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_fixed_gemini(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
void processor_fixed_http_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);
void processor_fixed_gopher_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);
void processor_fixed_finger_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);
void processor_fixed_gemini_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);
extern struct CharBuffer g_internal_buffer_one;
extern struct CharBuffer g_internal_buffer_two;

#endif
