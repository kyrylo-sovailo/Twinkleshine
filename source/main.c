#include "../include/server.h"
#include "../include/output.h"
#include "../commonlib/include/path.h"

static int common_main(int argc, cchar_t **argv)
{
    int code = 0;
    struct Error *error;
    
    output_module_initialize();
    PGOTO(path_module_initialize(argc, argv));
    PGOTO(server_main());
    error = OK;
    failure:
    if (error != OK)
    {
        error_print(error, NULL);
        code = error_get_exit_code(error);
        error_finalize(error);
    }
    path_module_finalize();
    output_module_finalize();

    return code;
}

int main(int argc, char **argv)
{
    return common_main(argc, argv);
}
