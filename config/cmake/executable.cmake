#####################
# Define executable #
#####################

# Define twinkleshine executable
devtemplate_add_executable(twinkleshine)
list(APPEND DEV_EXPORT_TARGETS twinkleshine)
list(APPEND DEV_CORE_TARGETS twinkleshine)
set_target_properties(twinkleshine PROPERTIES OUTPUT_NAME "$twinkleshine$<$<CONFIG:Debug>:-debug>")

# Define twinkleshine sources
devtemplate_target_sources(twinkleshine PRIVATE
source/char_buffer.c
source/client.c
source/error.c
source/generic_buffer_implementation.c
source/output.c
source/path.c
source/request_parser.c
source/request_processor.c
source/response_printer.c
source/string.c
source/twinkleshine.c
source/value.c)

# Define macros
target_compile_definitions(twinkleshine PUBLIC
    TWINKLESHINE_MAJOR=${DEV_MAJOR}
    TWINKLESHINE_MINOR=${DEV_MINOR}
    TWINKLESHINE_PATCH=${DEV_PATCH}
    TWINKLESHINE)