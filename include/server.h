#ifndef SERVER_H
#define SERVER_H

#include "../commonlib/include/macro.h"

struct Error;

struct Error *server_main(void) NODISCARD;

#endif
