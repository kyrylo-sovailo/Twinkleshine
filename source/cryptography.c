#include "../include/cryptography.h"
#include "../commonlib/include/error.h"
#include "../include/client.h"
#include "../include/constants.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <stddef.h>

/* https://hackmd.io/@oscarshiang/simple_tls_server */

static SSL_CTX *g_cryptography_context = NULL;

/*
static void cryptography_callback(const SSL *ssl, int where, int code)
{
    const char *string = "";
    if ((where & ~SSL_ST_MASK) & SSL_ST_CONNECT) string = "SSL_connect";
    else if ((where & ~SSL_ST_MASK) & SSL_ST_ACCEPT) string = "SSL_accept";

    if (where & SSL_CB_LOOP)
    {
        fprintf(stderr, "%s: %s\n", string, SSL_state_string_long(ssl));
    }
    else if (where & SSL_CB_ALERT)
    {
        fprintf(stderr, "ALERT: %s: %s\n", SSL_alert_type_string_long(code), SSL_alert_desc_string_long(code));
    }
    else if (where & SSL_CB_EXIT)
    {
        if (code == 0) fprintf(stderr, "%s: failed in %s\n", string, SSL_state_string_long(ssl));
        else if (code < 0) fprintf(stderr, "%s: error in %s\n", string, SSL_state_string_long(ssl));
    }
}
*/

static struct ExError cryptography_pump_to_ring(SSL *ssl, BIO *write_bio, struct Ring *ring, bool *connection_closed) NODISCARD;
static struct ExError cryptography_pump_to_ring(SSL *ssl, BIO *write_bio, struct Ring *ring, bool *connection_closed)
{
    /* The logic here is similar to server_receive_data + server_receive_value, except we don't know any kind of estimate */
    const struct ExError EXOK = { OK };
    struct Value value;
    while (true)
    {
        unsigned char i;
        EXPRETF(ring_reserve(ring, ring->size + MIN_AVAILABLE_REQUEST_STREAM), EEF_CLOSE_LOG);
        EXPRETF(ring_push_get(ring, MIN_AVAILABLE_REQUEST_STREAM, &value), EEF_CLOSE_LOG_DIE);
        for (i = 0; i < VALUE_PARTS; i++)
        {
            int signed_received;
            if (value.parts[i].size == 0) continue;
            if (ssl != NULL) signed_received = SSL_read(ssl, value.parts[i].p, (int)value.parts[i].size);
            else signed_received = BIO_read(write_bio, value.parts[i].p, (int)value.parts[i].size);
            if (signed_received > 0)
            {
                const size_t received = (size_t)signed_received;
                value.parts[i].p += received;
                value.parts[i].size -= received;
                if (value.parts[i].size > 0) goto breakbreak; /* TODO: is it though? Or should I wait hard fail? Read the documentation more carefully */
            }
            else if (ssl != NULL)
            {
                const int error = SSL_get_error(ssl, signed_received);
                EXARET0(error == SSL_ERROR_ZERO_RETURN || error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE, "SSL_read() failed", EEF_CLOSE_LOG);
                if (error == SSL_ERROR_ZERO_RETURN) *connection_closed = true;
                goto breakbreak;
            }
            else
            {
                goto breakbreak;
            }
        }
    }
    breakbreak:
    EXPRETF(ring_unpush(ring, value_size(&value)), EEF_CLOSE_LOG_DIE);
    return EXOK;
}

/* Extracts data from BIO to response stream, and also adds a phony frame (similar logic to processor_process) */
static struct ExError cryptography_create_phony_response(struct Client *client, size_t phony_size) NODISCARD;
static struct ExError cryptography_create_phony_response(struct Client *client, size_t phony_size)
{
    const struct ExError EXOK = { OK };
    struct Response phony = ZERO_INIT;
    if (phony_size == 0) return EXOK;
    phony.stream_size = phony_size;
    EXPRETF(ring_reserve(&client->response_queue, client->response_queue.size + sizeof(struct Response)), EEF_CLOSE_LOG);
    EXPRETF(ring_push_write(&client->response_queue, sizeof(struct Response), (const char*)&phony), EEF_CLOSE_LOG_DIE);
    if (client->response_count == 0) client->response = phony;
    client->response_count++;
    return EXOK;
}

struct Error *cryptography_module_initialize(void)
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    g_cryptography_context = SSL_CTX_new(TLS_server_method());
    ARET0(g_cryptography_context != NULL, "SSL_CTX_new() failed");
    ARET0(SSL_CTX_set_min_proto_version(g_cryptography_context, TLS1_2_VERSION) == 1, "SSL_CTX_set_min_proto_version() failed");
    ARET0(SSL_CTX_set_max_proto_version(g_cryptography_context, TLS1_3_VERSION) == 1, "SSL_CTX_set_max_proto_version() failed");
    if (SSL_CTX_use_certificate_file(g_cryptography_context, "/etc/ssl/certs/my8bitsoul.eu.crt", SSL_FILETYPE_PEM) == 1)
    {
        ARET0(SSL_CTX_use_PrivateKey_file(g_cryptography_context, "/etc/ssl/private/my8bitsoul.eu.key", SSL_FILETYPE_PEM) == 1, "SSL_CTX_use_PrivateKey_file() failed");
    }
    else
    {
        ARET0(SSL_CTX_use_PrivateKey_file(g_cryptography_context, "localhost.key", SSL_FILETYPE_PEM) == 1, "SSL_CTX_use_PrivateKey_file() failed");
        ARET0(SSL_CTX_use_certificate_file(g_cryptography_context, "localhost.crt", SSL_FILETYPE_PEM) == 1, "SSL_CTX_use_certificate_file() failed");
    }
    ARET0(SSL_CTX_check_private_key(g_cryptography_context) == 1, "SSL_CTX_check_private_key() failed");
    /* SSL_CTX_set_info_callback(g_cryptography_context, cryptography_callback); */
    return OK;
}

void cryptography_module_finalize(void)
{
    if (g_cryptography_context == NULL) return;
    SSL_CTX_free(g_cryptography_context);
    g_cryptography_context = NULL;
    EVP_cleanup();
}

struct ExError cryptography_initialize(struct Client *client)
{
    const struct ExError EXOK = { OK };
    struct ExError exerror;
    SSL *new_ssl = NULL;
    BIO *new_read_bio = NULL, *new_write_bio = NULL;
    bool bound = false;

    new_read_bio = BIO_new(BIO_s_mem());
    EXAGOTO0(new_read_bio != NULL, "BIO_new() failed", EEF_CLOSE_LOG);
    new_write_bio = BIO_new(BIO_s_mem());
    EXAGOTO0(new_write_bio != NULL, "BIO_new() failed", EEF_CLOSE_LOG);
    new_ssl = SSL_new(g_cryptography_context);
    EXAGOTO0(new_ssl != NULL, "SSL_new() failed", EEF_CLOSE_LOG);
    SSL_set_accept_state(new_ssl);
    SSL_set_bio(new_ssl, new_read_bio, new_write_bio);
    EXPRET(cryptography_pump_to_ring(NULL, new_write_bio, &client->response_stream, NULL));
    EXPRET(cryptography_create_phony_response(client, client->response_stream.size));
    bound = true;
    client->ssl = new_ssl;
    client->read_bio = new_read_bio;
    client->write_bio = new_write_bio;
    return EXOK;

    failure:
    if (new_ssl != NULL) SSL_free(new_ssl);
    if (!bound && new_read_bio != NULL) BIO_free(new_read_bio);
    if (!bound && new_write_bio != NULL) BIO_free(new_write_bio);
    return exerror;
}

struct ExError cryptography_finalize(struct Client *client)
{
    const struct ExError EXOK = { OK };
    size_t old_stream_size, new_stream_size, encrypted_value_size;
    if (client->cryptography_state != CS_SHUTDOWN)
    {
        const int code = SSL_shutdown(client->ssl);
        if (code <= 0) client->cryptography_state = CS_PENDING_SHUTDOWN;
        if (code > 0) client->cryptography_state = CS_SHUTDOWN;
        if (code == 0) { /*Do nothing */ }
        if (code < 0)
        {
            const int error = SSL_get_error(client->ssl, code);
            EXARET0(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE, "SSL_shutdown() failed", EEF_CLOSE_LOG);
        }

        old_stream_size = client->response_stream.size;
        EXPRET(cryptography_pump_to_ring(NULL, client->write_bio, &client->response_stream, NULL));
        new_stream_size = client->response_stream.size;
        encrypted_value_size = new_stream_size - old_stream_size;
        EXPRET(cryptography_create_phony_response(client, encrypted_value_size));
    }
    return EXOK;
}

struct ExError cryptography_decrypt(struct Client *client, size_t old_request_stream_size)
{
    const struct ExError EXOK = { OK };
    struct ValueLocation location; struct Value value;
    size_t old_stream_size, new_stream_size, encrypted_value_size;
    unsigned char i;

    /* Grab the latest encrypted data from request_stream and send it to SSL */
    location.offset = 0;
    location.size = client->request_stream.size - old_request_stream_size;
    EXPRETF(ring_get(&client->request_stream, &location, true, &value), EEF_CLOSE_LOG_DIE);
    for (i = 0; i < VALUE_PARTS; i++)
    {
        if (value.parts[i].size == 0) continue;
        EXARET0(BIO_write(client->read_bio, value.parts[i].p, (int)value.parts[i].size) == (int)value.parts[i].size, "BIO_write() failed", EEF_CLOSE_LOG);
    }
    EXPRETF(ring_unpush(&client->request_stream, location.size), EEF_CLOSE_LOG_DIE);

    /* Read decrypted data from SSL and place it in request stream */
    if (client->cryptography_state == CS_PENDING_HANDSHAKE)
    {
        const int code = SSL_accept(client->ssl);
        if (code > 0) client->cryptography_state = CS_OPERATIONAL;
        if (code == 0) client->cryptography_state = CS_SHUTDOWN;
        if (code < 0)
        {
            const int error = SSL_get_error(client->ssl, code);
            EXARET0(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE, "SSL_accept() failed", EEF_CLOSE_LOG);
        }
    }
    if (client->cryptography_state == CS_OPERATIONAL)
    {
        bool connection_closed = false;
        EXPRET(cryptography_pump_to_ring(client->ssl, NULL, &client->request_stream, &connection_closed));
        if (connection_closed) client->cryptography_state = CS_SHUTDOWN;
    }
    if (client->cryptography_state == CS_PENDING_SHUTDOWN)
    {
        const int code = SSL_shutdown(client->ssl);
        if (code > 0) client->cryptography_state = CS_SHUTDOWN;
        if (code == 0) { /*Do nothing */ }
        if (code < 0)
        {
            const int error = SSL_get_error(client->ssl, code);
            EXARET0(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE, "SSL_shutdown() failed", EEF_CLOSE_LOG);
        }
    }

    /* Read encrypted data from SSL and place it in response stream */
    old_stream_size = client->response_stream.size;
    EXPRET(cryptography_pump_to_ring(NULL, client->write_bio, &client->response_stream, NULL));
    new_stream_size = client->response_stream.size;
    encrypted_value_size = new_stream_size - old_stream_size;
    EXPRET(cryptography_create_phony_response(client, encrypted_value_size));
    return EXOK;
}

struct ExError cryptography_encrypt(struct Client *client, const struct Response *response, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    size_t old_stream_size, new_stream_size, encrypted_value_size;
    struct ValueLocation location; struct Value value;
    unsigned char i;

    /* Encrypt data and place it in response_stream */
    for (i = 0; i < VALUE_PARTS; i++)
    {
        if (response_stream->parts[i].size == 0) continue;
        EXARET0(SSL_write(client->ssl, response_stream->parts[i].p, (int)response_stream->parts[i].size) == (int)response_stream->parts[i].size, "BIO_write() failed", EEF_CLOSE_LOG);
    }
    old_stream_size = client->response_stream.size;
    EXPRET(cryptography_pump_to_ring(NULL, client->write_bio, &client->response_stream, NULL)); /* No phony request because  */
    new_stream_size = client->response_stream.size;
    encrypted_value_size = new_stream_size - old_stream_size;

    /* Correct the size of the response (this is where the clown show begins) */
    location.offset = response->size + (sizeof(struct Response) - offsetof(struct Response, stream_size) - sizeof(response->stream_size));
    location.size = sizeof(response->stream_size);
    EXPRETF(ring_get(&client->response_queue, &location, true, &value), EEF_CLOSE_LOG_DIE);
    value_write(&value, (const char*)&encrypted_value_size);
    if (client->response_count == 0) client->response.stream_size = encrypted_value_size;
    
    return EXOK;
}
