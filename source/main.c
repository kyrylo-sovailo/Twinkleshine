#include "../include/server.h"
#include "../include/output.h"
#include "../include/tables.h"

int main(int argc, char **argv)
{
    int code = 0;
    struct Error *error;

    tables_initialize();
    output_initialize();
    error = server_main(argc, argv);
    if (error != OK)
    {
        error_print(error);
        code = error_get_exit_code(error);
        error_finalize(error);
    }
    output_finalize();
    return code;
}