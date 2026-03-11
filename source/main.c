#include "../include/server.h"

int main(int argc, char **argv)
{
    int code = 0;
    struct Error *error = server_main(argc, argv);
    if (error != OK)
    {
        error_print(error);
        code = error_get_exit_code(error);
        error_finalize(error);
    }
    return code;
}