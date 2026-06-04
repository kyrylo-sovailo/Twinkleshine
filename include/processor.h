#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "../commonlib/include/macro.h"
#include "../include/extended_error.h"
#include "../include/value.h"

#include <stdarg.h>

struct CharBuffer;
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
    bool phony;                     /* Phony, print silently */
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
enum EntryStyle
{
    ES_INITIALIZE,
    ES_FINALIZE,
    ES_NORMAL,
    ES_ITEMIZE,
    ES_ENUMERATION,
    ES_QUOTE,
    ES_LARGE,
    ES_LARGER,
    ES_LARGEST,
    ES_HEADER,
    ES_INTERNAL_REFERENCE,
    ES_EXTERNAL_REFERENCE
};

struct ProcessorPrintContext
{
    struct CharBuffer *one, *two;
    const struct Request *request;
    enum EntryStyle previous_style;
    unsigned int list_index;
};

struct Error *processor_module_initialize_http(void);
void processor_module_finalize_http(void);

struct Error *processor_print_http(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_gopher(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_finger(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_gemini(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_spartan(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_nex(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;

struct ExError processor_process_http(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_process_gopher(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_process_finger(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_process_gemini(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_process_spartan(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_process_nex(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;

struct ExError processor_fixed_http(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_fixed_gopher(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_fixed_finger(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_fixed_gemini(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_fixed_spartan(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;
struct ExError processor_fixed_nex(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream) NODISCARD;

void processor_fixed_http_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);
void processor_fixed_gopher_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);
void processor_fixed_finger_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);
void processor_fixed_gemini_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);
void processor_fixed_spartan_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);
void processor_fixed_nex_failsafe(enum FixedResponse fixed, struct ConstValue *response_stream);

struct ExError processor_construct_response(struct Response *response, struct Ring *response_queue,
    size_t stream_size, bool keep_alive,
    const struct Value *method, const struct Value *resource, const struct Value *protocol) NODISCARD;

extern struct CharBuffer g_internal_buffer_one;
extern struct CharBuffer g_internal_buffer_two;

#endif
