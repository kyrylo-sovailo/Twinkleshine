#ifndef CRYPTOGRAPHY_H
#define CRYPTOGRAPHY_H

#include "../commonlib/include/macro.h"
#include "../include/extended_error.h"

#include <stddef.h>

struct Client;
struct ConstValue;
struct Response;

/* Initializes the module */
struct Error *cryptography_module_initialize(void) NODISCARD;

/* Finalizes the module */
void cryptography_module_finalize(void);

/* Initializes cryptography for one client */
struct ExError cryptography_initialize(struct Client *client) NODISCARD;

/* Decodes the data placed in request_stream (and also places data in response stream) */
struct ExError cryptography_decrypt(struct Client *client, size_t old_request_stream_size) NODISCARD;

/* Encrypts the data and places it in request stream, and also corrects the stream size in request/request queue */
struct ExError cryptography_encrypt(struct Client *client, const struct Response *response, struct ConstValue *response_stream) NODISCARD;

#endif
