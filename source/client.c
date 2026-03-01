#include "../include/client.h"
#include "../include/buffer_implementation.h"

IMPLEMENT_BUFFER_RESIZE(struct Client, ClientBuffer, clients_)
IMPLEMENT_BUFFER_FINALIZE(struct Client, ClientBuffer, clients_)

IMPLEMENT_BUFFER_RESIZE(struct pollfd, PollBuffer, polls_)
IMPLEMENT_BUFFER_FINALIZE(struct pollfd, PollBuffer, polls_)
