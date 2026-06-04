#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "macro.h"

/* Units */
#define KILOBYTE 1024
#define MEGABYTE 1048576
#define GIGABYTE 1073741824
#define SECOND 1
#define MINUTE 60
#define HOUR 3600
#define DAY 86400
#define WEEK 604800

/*
Throttling and security:

- 1. Max clients in queue, needed by the OS 
- 2. Start sending 500 and closing incoming connections immediately when:
    - 1. Maximum number of clients if reached
    - 2. Maximum memory usage is reached
    - 3. Maximum processor utilization is reached
- 3. Stop accepting data from a client when:
    - 1. Maximum size of input buffer is reached
    - 2. Certain size of output buffer is reached
- 4. Send client 500 and close connection when:
    - 1. Error occurred
    - 2. Client sent more than certain amount of data at once
    - 3. Client promised to send a request larger than certain amount
    - 4. Input buffer is incomplete for certain time
- 5. Close connection without sending 500 when:
    - 1. 4.1 but output buffer is not empty
    - 2. 4.2 but output buffer is not empty
    - 3. 4.3 but output buffer is not empty
    - 4. 4.4 but output buffer is not empty
    - 5. Input buffer is empty for a certain amount of time
    - 6. Output buffer is full for certain time
    - 7. Output buffer is incomplete for a certain time
*/

/* 1.x */
#define MAX_CLIENTS_IN_QUEUE            16              /* 1.1 */

/* 2.x */
#define MAX_CLIENTS                     256             /* 2.1 */
#define MAX_MEMORY_USAGE                (1 * GIGABYTE)  /* 2.2 */
#define MAX_PROCESSOR_UTILIZATION       0.95            /* 2.3 */
#define PROCESSOR_UTILIZATION_UPD       0.05

/* 3.x */
#define MAX_REQUEST_STREAM_SIZE         (1 * MEGABYTE)  /* 3.1 */
#define MAX_RESPONSE_STREAM_SIZE        (1 * MEGABYTE)  /* 3.2 */

/* 4.x */
#define MAX_AVAILABLE_REQUEST_STREAM    (2 * MEGABYTE)  /* 4.2 */
#define MAX_REQUEST_SIZE                (1 * MEGABYTE)  /* 4.3 */
#define MAX_INCOMPLETE_REQUEST_TIME     (1 * MINUTE)    /* 4.4 */

/* 5.x */
#define MAX_EMPTY_REQUEST_STREAM_TIME   (10 * MINUTE)   /* 5.5 */
#define MAX_FULL_RESPONSE_STREAM_TIME   (1 * MINUTE)    /* 5.6 */
#define MAX_INCOMPLETE_RESPONSE_TIME    (1 * MINUTE)    /* 5.7 */

#if MAX_REQUEST_SIZE > MAX_REQUEST_STREAM_SIZE
    /*
    MAX_REQUEST_STREAM_SIZE is the size of the buffer that gets allocated
    MAX_REQUEST_SIZE is the size maximum size of request
    Does it make sense to allocate more buffer then the request size?
    Not really, considering that there's probably another buffer behind read()
    But I will keep these two values separate
    */
    #error "Request can be larger than input buffer"
#endif

#define MIN_AVAILABLE_REQUEST_STREAM 4096
#if MIN_AVAILABLE_REQUEST_STREAM > MAX_AVAILABLE_REQUEST_STREAM
    /*
    MIN_AVAILABLE_REQUEST_STREAM is how bytes must be read regardless what ioctl says
    Serves to circumvent unreliable ioctl and to detect normal terminations
    */
    #error "Minimal available input is more than maximal available input"
#endif

/*
Logging

- 1. Delete oldest log if the total number of logs is bigger than a certain number
- 2. Delete oldest log if the total size logs is bigger than a certain size
- 3. Delete log if it is older than certain time
- 4. Stop writing to log and append suffix if log is larger than a certain size
*/

#define MAX_TOTAL_LOG_NUMBER  16              /* 1 */
#define MAX_TOTAL_LOG_SIZE    (16 * MEGABYTE) /* 2 */
#define MAX_LOG_AGE           (1 * WEEK)      /* 3 */
#define MAX_LOG_SIZE          (1 * MEGABYTE)  /* 4 */

/*
Socket allocation
*/

#ifdef TWINKLESHINE_RELEASE
    #define HTTP_PORT 80
    #define HTTPS_PORT 443
    #define GOPHER_PORT 70
    #define FINGER_PORT 79
    #define GEMINI_PORT 1965
    #define NEX_PORT 1900
    #define DOMAIN_NAME "my8bitsoul.eu"
    #define KEY_FILE "/etc/ssl/private/my8bitsoul.eu.key"
    #define CERTIFICATE_FILE "/etc/ssl/certs/my8bitsoul.eu.crt"
#else
    #define HTTP_PORT 8080
    #define HTTPS_PORT 8443
    #define GOPHER_PORT 8070
    #define FINGER_PORT 8079
    #define GEMINI_PORT 9965
    #define NEX_PORT 9900
    #define DOMAIN_NAME "localhost"
    #define KEY_FILE "localhost.key"
    #define CERTIFICATE_FILE "localhost.crt"
#endif
#if HTTP_PORT == 80
    #define HTTP_PORT_STRING ""
#else
    #define HTTP_PORT_STRING ":" STRINGIZE(HTTP_PORT)
#endif
#if HTTPS_PORT == 443
    #define HTTPS_PORT_STRING ""
#else
    #define HTTPS_PORT_STRING ":" STRINGIZE(HTTPS_PORT)
#endif
#if GOPHER_PORT == 70
    #define GOPHER_PORT_STRING ""
#else
    #define GOPHER_PORT_STRING ":" STRINGIZE(GOPHER_PORT)
#endif
#if FINGER_PORT == 79
    #define FINGER_PORT_STRING ""
#else
    #define FINGER_PORT_STRING ":" STRINGIZE(FINGER_PORT)
#endif
#if GEMINI_PORT == 1965
    #define GEMINI_PORT_STRING ""
#else
    #define GEMINI_PORT_STRING ":" STRINGIZE(GEMINI_PORT)
#endif
#if NEX_PORT == 1900
    #define NEX_PORT_STRING ""
#else
    #define NEX_PORT_STRING ":" STRINGIZE(NEX_PORT)
#endif

#define ACCEPTING_SOCKETS 12
#define ACCEPTING_SOCKET_IS_HTTP(INDEX)   (                 (INDEX) < 2 )
#define ACCEPTING_SOCKET_IS_HTTPS(INDEX)  ((INDEX) >= 2  && (INDEX) < 4 )
#define ACCEPTING_SOCKET_IS_GOPHER(INDEX) ((INDEX) >= 4  && (INDEX) < 6 )
#define ACCEPTING_SOCKET_IS_FINGER(INDEX) ((INDEX) >= 6  && (INDEX) < 8 )
#define ACCEPTING_SOCKET_IS_GEMINI(INDEX) ((INDEX) >= 8  && (INDEX) < 10)
#define ACCEPTING_SOCKET_IS_NEX(INDEX)    ((INDEX) >= 10                )

#endif
