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
    struct ValueLocation chunks;    /* Array of unsigned chars, how many times chunks were sent */
    bool keep_alive;                /* Keep connection alive */
    bool silent;                    /* Phony, print silently */
    unsigned int first_chunk;       /* Sequence number of the first chunk */
};

/* Style */
enum EntryStyle
{
    ES_INITIALIZE,
    ES_FINALIZE,
    ES_NORMAL,
    ES_PARAGRAPH,
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
    /* Technical details */
    struct CharBuffer *one, *two;
    const struct Request *request;

    /* Pretty text formatting */
    enum EntryStyle previous_style;
    unsigned int list_index;
    char language, reference_language;

    /* Message-based stuff */
    unsigned int first_chunk, chunk_number;
};

/* processor.c */
struct Error *processor_module_initialize(void) NODISCARD;
void processor_module_finalize(void);
void processor_free(void);
struct ExError processor_construct_response(struct Response *response, struct Ring *response_queue,
    size_t stream_size, bool keep_alive, unsigned int first_chunk, unsigned int chunk_number,
    const struct ConstantValue *method, const struct ConstantValue *resource, const struct ConstantValue *protocol) NODISCARD;

struct ExError processor_process(unsigned char accepting_socket, const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;
struct ExError processor_fixed(unsigned char accepting_socket, enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream) NODISCARD;
void processor_fixed_failsafe(unsigned char accepting_socket, enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream);
struct Error *processor_print(unsigned char accepting_socket, struct ProcessorPrintContext *context,
    enum EntryStyle style, const char *resource, const char *format, ...) PRINTFLIKE(5, 6) NODISCARD;

/* ${PROTOCOL}_processor.c */
struct Error *processor_module_initialize_http(void);
void processor_module_finalize_http(void);

/* Processes request and creates response (response + metadata + stream) */
struct ExError processor_process_http(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;
struct ExError processor_process_gopher(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;
struct ExError processor_process_finger(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;
struct ExError processor_process_gemini(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;
struct ExError processor_process_spartan(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;
struct ExError processor_process_nex(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;
struct ExError processor_process_text(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;
struct ExError processor_process_guppy(const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstantValue *response_stream) NODISCARD;

/* Creates fixed response (response + metadata + stream) */
struct ExError processor_fixed_http(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream) NODISCARD;
struct ExError processor_fixed_gopher(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream) NODISCARD;
struct ExError processor_fixed_finger(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream) NODISCARD;
struct ExError processor_fixed_gemini(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream) NODISCARD;
struct ExError processor_fixed_spartan(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream) NODISCARD;
struct ExError processor_fixed_nex(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream) NODISCARD;
struct ExError processor_fixed_text(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream) NODISCARD;
struct ExError processor_fixed_guppy(enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstantContinuousValue *response_stream) NODISCARD;

/* Creates fixed response very fast and without errors */
void processor_fixed_http_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream);
void processor_fixed_gopher_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream);
void processor_fixed_finger_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream);
void processor_fixed_gemini_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream);
void processor_fixed_spartan_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream);
void processor_fixed_nex_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream);
void processor_fixed_text_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream);
void processor_fixed_guppy_failsafe(enum FixedResponse fixed,
    struct ConstantContinuousValue *response_stream);

/* Prints an entry into the buffer */
struct Error *processor_print_http(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_gopher(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_finger(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_gemini(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_spartan(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_nex(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_text(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;
struct Error *processor_print_guppy(struct ProcessorPrintContext *context, enum EntryStyle style, const char *resource, const char *format, va_list va) PRINTFLIKE(4, 0) NODISCARD;

/* generic.c */
bool processor_generic_paragraph(enum EntryStyle style);
void processor_generic_upper_case(struct CharBuffer *string, size_t old_string_size, size_t prefix_size, size_t suffix_size);
struct Error *processor_generic_box(struct CharBuffer *string, size_t old_string_size, size_t prefix_size, size_t suffix_size, char c1, char c2) NODISCARD;
struct Error *processor_generic(unsigned char accepting_socket, struct ProcessorPrintContext *context, const struct ConstantValue *resource, bool *invalid_resource) NODISCARD;

#endif
