#include "../../include/processor.h"
#include "../../commonlib/include/string.h"
#include "../../include/client.h"

struct CharBuffer g_internal_buffer_one = ZERO_INIT;
struct CharBuffer g_internal_buffer_two = ZERO_INIT;

struct Error *processor_module_initialize(void)
{
    PRET(processor_module_initialize_http());
    return OK;
}

void processor_module_finalize(void)
{
    processor_module_initialize_http();
}

struct ExError processor_process(int type, const struct Request *request, const struct Ring *request_stream,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    switch (type)
    {
    case CT_HTTP:   EXPRET(processor_process_http(request, request_stream, response, response_queue, response_stream));     break;
    case CT_GOPHER: EXPRET(processor_process_gopher(request, request_stream, response, response_queue, response_stream));   break;
    case CT_FINGER: EXPRET(processor_process_finger(request, request_stream, response, response_queue, response_stream));   break;
    default:        EXPRET(processor_process_gemini(request, request_stream, response, response_queue, response_stream));   break; /* CT_GEMINI */
    }
    return EXOK;
}

struct ExError processor_fixed(int type, enum FixedResponse fixed,
    struct Response *response, struct Ring *response_queue, struct ConstValue *response_stream)
{
    const struct ExError EXOK = { OK };
    switch (type)
    {
    case CT_HTTP:   EXPRET(processor_fixed_http(fixed, response, response_queue, response_stream));     break;
    case CT_GOPHER: EXPRET(processor_fixed_gopher(fixed, response, response_queue, response_stream));   break;
    case CT_FINGER: EXPRET(processor_fixed_finger(fixed, response, response_queue, response_stream));   break;
    default:        EXPRET(processor_fixed_gemini(fixed, response, response_queue, response_stream));   break; /* CT_GEMINI */
    }
    return EXOK;
}

void processor_fixed_failsafe(int type, enum FixedResponse fixed, struct ConstValue *response_stream)
{
    switch (type)
    {
    case CT_HTTP:   processor_fixed_http_failsafe(fixed, response_stream);      break;
    case CT_GOPHER: processor_fixed_gopher_failsafe(fixed, response_stream);    break;
    case CT_FINGER: processor_fixed_finger_failsafe(fixed, response_stream);    break;
    default:        processor_fixed_gemini_failsafe(fixed, response_stream);    break; /* CT_GEMINI */
    }
}

void processor_free(void)
{
    /* Do nothing */
}