#ifndef REQUEST_PARSER_H
#define REQUEST_PARSER_H

#include "../commonlib/include/macro.h"
#include "request.h"
#include "value.h"

struct Error;
struct Ring;

/* Client's state machine */
enum RequestParserState
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

/* Client's state (except the memory itself) */
struct RequestParser
{
    struct Request request;             /* Current output */
    size_t offset;                      /* Number of bytes consumed aka request size */
    enum RequestParserState state;      /* Current state */
    unsigned int remaining_content;     /* Expected content length */
    bool tolerated_lf, tolerated_cr;    /* Flags that indicate non-conforming implementations, used to avoid waiting for line ends that aren't coming */
    struct ValueLocation current_name;  /* Field name */
    struct ValueLocation current_value; /* Field value */
};

/* Feed buffer to parser */
struct Error *request_parser_parse(struct RequestParser *parser, const struct Ring *input) NODISCARD;

#endif
