#ifndef REQUEST_H
#define REQUEST_H

#include "value.h"

/* Request fields that are necessary to form response (except the memory itself) */
struct Request
{
    struct ValueLocation method;    /* Requested method */
    struct ValueLocation resource;  /* Requested resource */
    struct ValueLocation protocol;  /* Protocol version */
    bool keep_alive;                /* Keep connection alive */

    struct ValueLocation content;   /* Content */
};

#endif
