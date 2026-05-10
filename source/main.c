#include "../commonlib/include/error.h"
#include "../include/output.h"
#include "../include/server.h"

#include <stddef.h>
#include <string.h>

static int common_main(int argc, char **argv)
{
    int code = 0;
    struct Error *error;

    if (argc > 0 && argv[0] != NULL)
    {
        g_application = strrchr(argv[0], '/');
        if (g_application != NULL) g_application++;
        else g_application = argv[0];
    }
    
    output_module_initialize();
    PGOTO(server_main());
    error = OK;
    failure:
    if (error != OK)
    {
        error_print(error, NULL);
        code = error_get_exit_code(error);
        error_finalize(error);
    }
    output_module_finalize();

    return code;
}

int main(int argc, char **argv)
{
    return common_main(argc, argv);
}
