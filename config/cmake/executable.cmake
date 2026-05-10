#####################
# Define executable #
#####################

# Define twinkleshine executable
devtemplate_add_executable(twinkleshine)
list(APPEND DEV_EXPORT_TARGETS twinkleshine)
list(APPEND DEV_CORE_TARGETS twinkleshine)
set_target_properties(twinkleshine PROPERTIES OUTPUT_NAME "twinkleshine$<$<CONFIG:Debug>:-debug>")

# Define twinkleshine sources
devtemplate_target_sources(twinkleshine PRIVATE
source/client.c
source/extended_error.c
source/main.c
source/memory.c
source/output.c
source/parser.c
source/processor.c
source/random.c
source/ring.c
source/server.c
source/tables.c
source/value.c
commonlib/source/char_buffer.c
commonlib/source/error.c
commonlib/source/generic_buffer_implementation.c
commonlib/source/string.c
)

# Define macros
target_compile_definitions(twinkleshine PUBLIC
    TWINKLESHINE_MAJOR=${DEV_MAJOR}
    TWINKLESHINE_MINOR=${DEV_MINOR}
    TWINKLESHINE_PATCH=${DEV_PATCH}
    TWINKLESHINE)