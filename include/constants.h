#ifndef CONSTANTS_H
#define CONSTANTS_H

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
    - 3. Client promised to send a request larger than certain amount (TODO: differentiate between too large header and too large body)
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
#define MAX_CLIENTS_IN_QUEUE 16 /* 1.1 */

/* 2.x */
#define MAX_CLIENTS                 256             /* 2.1 */
#define MAX_MEMORY_USAGE            (1 * GIGABYTE)  /* 2.2 */
#define MAX_PROCESSOR_UTILIZATION   0.95            /* 2.3 */

/* 3.x */
#define MAX_INPUT_BUFFER_SIZE       (1 * MEGABYTE)  /* 3.1 */
#define MAX_OUTPUT_BUFFER_SIZE      (1 * MEGABYTE)  /* 3.2 */

/* 4.x */
#define MAX_AVAILABLE_INPUT         (2 * MEGABYTE)  /* 4.2 */
#define MAX_REQUEST_SIZE            (1 * MEGABYTE)  /* 4.3 */
#define MAX_INCOMPLETE_INPUT_TIME   (1 * MINUTE)    /* 4.4 */

/* 5.x */
#define MAX_NO_INPUT_TIME           (10 * MINUTE)   /* 5.5 */
#define MAX_FULL_OUTPUT_TIME        (1 * MINUTE)    /* 5.6 */
#define MAX_INCOMPLETE_OUTPUT_TIME  (1 * MINUTE)    /* 5.7 */

#if MAX_REQUEST_SIZE > MAX_INPUT_BUFFER_SIZE
    /*
    MAX_INPUT_BUFFER_SIZE is the size of the buffer that gets allocated
    MAX_REQUEST_SIZE is the size maximum size of request
    Does it make sense to allocate more buffer then the request size?
    Not really, considering that there's probably another buffer behind read()
    But I will keep these two values separate
    */
    #error "Request can be larger than input buffer"
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

#endif
