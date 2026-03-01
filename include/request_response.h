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

    struct Value method;            /* Requested method */
    struct Value resource;          /* Requested resource */
    struct Value protocol;          /* Protocol version */
    bool keep_alive;                /* Keep connection alive */

    struct Value content;           /* Content */
};

/* Response fields that are necessary to form response text */
struct Response
{
    bool success;                   /* Status */
    bool keep_alive;                /* Keep connection alive */

    struct CharBuffer content;      /* Content */
};

#endif