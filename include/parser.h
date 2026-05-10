#ifndef PARSER_H
#define PARSER_H

#include "../commonlib/include/macro.h"
#include "value.h"
#include "extended_error.h"

struct Ring;

/* Request fields that are necessary to form response */
struct Request
{
    size_t stream_size;             /* Total request size */
    struct ValueLocation method;    /* Requested method */
    struct ValueLocation resource;  /* Requested resource */
    struct ValueLocation protocol;  /* Protocol version */
    bool keep_alive;                /* Keep connection alive */

    struct ValueLocation content;   /* Content */
};

/* Client's state machine */
enum ParserState
{
    RPS_WAIT_METHOD_BEGIN = 0,
    RPS_WAIT_METHOD_END,
    RPS_WAIT_RESOURCE_BEGIN,
    RPS_WAIT_RESOURCE_END,
    RPS_WAIT_PROTOCOL_BEGIN,
    RPS_WAIT_PROTOCOL_END,
    RPS_WAIT_LINE_CR,
    RPS_WAIT_LINE_LF,

    RPS_WAIT_NAME_BEGIN,
    RPS_WAIT_NAME_END,
    RPS_WAIT_COLON,
    RPS_WAIT_VALUE_END,

    RPS_WAIT_FINAL_LINE_LF,
    RPS_WAIT_CONTENT,
    RPS_END
};

/* Client's full state machine, complements Request */
struct Parser
{
    enum ParserState state;             /* Current state */
    unsigned int remaining_content;     /* Expected content length */
    bool tolerated_lf, tolerated_cr;    /* Flags that indicate non-conforming implementations, used to avoid waiting for line ends that aren't coming */
    struct ValueLocation current_name;  /* Field name */
    struct ValueLocation current_value; /* Field value */
};

/* Feed buffer to parser */
struct ExError parser_parse(struct Parser *parser, struct Request *request, const struct Ring *request_stream) NODISCARD;

#endif
