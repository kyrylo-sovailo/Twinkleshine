#ifndef REQUEST_H
#define REQUEST_H

#include "bool.h"
#include "error.h"
#include "string.h"
#include "value.h"

#include <time.h>

/* Request fields that are necessary to form response */
struct Request
{
    struct CharBuffer data;         /* Owner of all memory */

    struct ValueRange method;       /* Requested method */
    struct ValueRange resource;     /* Requested resource */
    struct ValueRange protocol;     /* Protocol version */
    bool keep_alive;                /* Keep connection alive */

    struct ValueRange content;      /* Content */
};

/* Response fields that are necessary to form response text */
struct Response
{
    bool success;                   /* Status */
    bool keep_alive;                /* Keep connection alive */

    struct CharBuffer content;      /* Content */
};

#endif