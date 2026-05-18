#include "../include/cryptography.h"
#include "../commonlib/include/error.h"
#include "../include/client.h"
#include "../include/constants.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

static SSL_CTX *g_cryptography_context = NULL;

static struct ExError cryptography_pump_to_ring(SSL *ssl, BIO *write_bio, struct Ring *ring, bool *connection_closed)
{
    /* The logic here is similar to server_receive_data + server_receive_value, except we don't know any kind of estimate */
    const struct ExError EXOK = { OK };
    unsigned char i;
    while (true)
    {
        struct Value value;
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
                if (received < value.parts[i].size) /* TODO: is it though? Read the documentation more carefully */
                {
                    EXPRETF(ring_unpush(ring, value.parts[i].size - received), EEF_CLOSE_LOG_DIE);
                    return EXOK;
                }
            }
            else if (ssl != NULL)
            {
                const int error = SSL_get_error(ssl, signed_received);
                EXARET0(error == SSL_ERROR_ZERO_RETURN || error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE, "SSL_read() failed", EEF_CLOSE_LOG);
                EXPRETF(ring_unpush(ring, value.parts[i].size), EEF_CLOSE_LOG_DIE);
                if (error == SSL_ERROR_ZERO_RETURN) *connection_closed = true;
                return EXOK;
            }
            else
            {
                return EXOK;
            }
        }
    }
}

struct Error *cryptography_module_initialize(void)
{
    ARET0(OPENSSL_init_ssl(0, NULL) >= 0, "OPENSSL_init_ssl() failed");
    g_cryptography_context = SSL_CTX_new(TLS_server_method());
    ARET0(g_cryptography_context != NULL, "SSL_CTX_new() failed");
    if (SSL_CTX_use_certificate_file(g_cryptography_context, "/etc/ssl/certs/my8bitsoul.eu.crt", SSL_FILETYPE_PEM) >= 0)
    {
        ARET0(SSL_CTX_use_PrivateKey_file(g_cryptography_context, "/etc/ssl/private/my8bitsoul.eu.key", SSL_FILETYPE_PEM) >= 0, "SSL_CTX_use_PrivateKey_file() failed");
    }
    else
    {
        ARET0(SSL_CTX_use_PrivateKey_file(g_cryptography_context, "/etc/ssl/private/localhost.key", SSL_FILETYPE_PEM) >= 0, "SSL_CTX_use_PrivateKey_file() failed");
        ARET0(SSL_CTX_use_certificate_file(g_cryptography_context, "/etc/ssl/certs/localhost.crt", SSL_FILETYPE_PEM) >= 0, "SSL_CTX_use_certificate_file() failed");
    }
    ARET0(SSL_CTX_set_ecdh_auto(g_cryptography.context, 1) >= 0, "SSL_CTX_set_ecdh_auto() failed");
    return OK;
}

void cryptography_module_finalize(void)
{
    if (g_cryptography_context != NULL) SSL_CTX_free(g_cryptography_context);
}

struct Error *cryptography_initialize(struct Client *client)
{
    struct Error *error;
    SSL *new_ssl = NULL;
    BIO *new_read_bio = NULL, *new_write_bio = NULL;
    bool bound = false;

    new_ssl = SSL_new(g_cryptography_context);
    AGOTO0(new_ssl != NULL, "SSL_new() failed");
    new_read_bio = BIO_new(BIO_s_mem());
    AGOTO0(new_read_bio != NULL, "BIO_new() failed");
    new_write_bio = BIO_new(BIO_s_mem());
    AGOTO0(new_write_bio != NULL, "BIO_new() failed");
    AGOTO0(BIO_set_nbio(new_read_bio, 1) >= 0, "BIO_set_nbio() failed");
    AGOTO0(BIO_set_nbio(new_write_bio, 1) >= 0, "BIO_set_nbio() failed");
    SSL_set_bio(new_ssl, new_read_bio, new_write_bio);
    bound = true;
    SSL_set_accept_state(new_ssl);
    client->ssl = new_ssl;
    client->read_bio = new_read_bio;
    client->write_bio = new_write_bio;
    return OK;

    failure:
    if (new_ssl != NULL) SSL_free(new_ssl);
    if (!bound && new_read_bio != NULL) BIO_free(new_read_bio);
    if (!bound && new_write_bio != NULL) BIO_free(new_write_bio);
    return error;
}

struct ExError cryptography_decrypt(struct Client *client, size_t old_request_stream_size)
{
    const struct ExError EXOK = { OK };
    struct ValueLocation location; struct Value value;
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
        else if (code == 0) client->cryptography_state = CS_SHUTDOWN; /* Controlled shut down? Okay, as you wish. */
        else
        {
            const int error = SSL_get_error(client->ssl, code);
            EXARET0(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE, "SSL_accept() failed", EEF_CLOSE_LOG);
        }
    }
    if (client->cryptography_state == CS_OPERATIONAL)
    {
        bool connection_closed = false;
        EXPRET(cryptography_pump_to_ring(client->ssl, NULL, &client->request_stream, &connection_closed));

    }
    else if (client->cryptography_state == CS_PENDING_SHUTDOWN)
    {
        const int code = SSL_shutdown(client->ssl);
        if (code == 0) { /*Do nothing */ }
        else if (code > 0)
        {
            client->cryptography_state = CS_SHUTDOWN;
        }
        else
        {
            const int error = SSL_get_error(client->ssl, code);
            EXARET0(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE, "SSL_shutdown() failed", EEF_CLOSE_LOG);
        }
    }

    /* Read encrypted data from SSL and place it in response stream */
    EXPRET(cryptography_pump_to_ring(NULL, client->write_bio, &client->response_stream, NULL));
    return EXOK;
}

struct ExError cryptography_encrypt(struct Client *client, struct ConstValue *value)
{
    const struct ExError EXOK = { OK };
    const size_t old_response_stream_size = client->response_stream.size;
    unsigned char i;

    for (i = 0; i < VALUE_PARTS; i++)
    {
        if (value->parts[i].size == 0) continue;
        EXARET0(SSL_write(client->ssl, value->parts[i].p, (int)value->parts[i].size) == (int)value->parts[i].size, "BIO_write() failed", EEF_CLOSE_LOG);
    }

    EXPRET(cryptography_pump_to_ring(NULL, client->write_bio, &client->response_stream, NULL));

    if (client->response_queue.size > 0)
    {
        struct ValueLocation location; struct Value value;
        struct Response response;
        location.offset = 0;
        location.size = sizeof(struct Response);
        EXPRETF(ring_get(&client->response_queue, &location, true, &value), EEF_CLOSE_LOG_DIE);
        value_read(&value, (char*)&response);
        response.stream_size = old_response_stream_size - client->response_stream.size;
        value_write(&value, (const char*)&response);
    }
    else
    {
        client->response.stream_size = old_response_stream_size - client->response_stream.size;
    }
    return EXOK;
}
