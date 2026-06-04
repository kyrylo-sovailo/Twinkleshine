#####################
# Define executable #
#####################

# Dependencies
find_package(OpenSSL COMPONENTS SSL)

# Define twinkleshine executable
devtemplate_add_executable(twinkleshine)
list(APPEND DEV_EXPORT_TARGETS twinkleshine)
list(APPEND DEV_CORE_TARGETS twinkleshine)
set_target_properties(twinkleshine PROPERTIES OUTPUT_NAME "twinkleshine$<$<CONFIG:Debug>:-debug>")

# Link twinkleshine dependencies
target_link_libraries(twinkleshine PRIVATE OpenSSL::SSL)

# Define twinkleshine sources
devtemplate_target_sources(twinkleshine PRIVATE
commonlib/source/generic_buffer_implementation.c
commonlib/source/char_buffer.c
commonlib/source/string.c
commonlib/source/error.c
source/server/initialize.c
source/server/receive.c
source/server/high_level.c
source/server/main.c
source/server/send.c
source/server/process.c
source/server/low_level.c
source/server/accept.c
source/cryptography.c
source/parser/parser.c
source/parser/gopher_parser.c
source/parser/spartan_parser.c
source/parser/http_parser.c
source/parser/finger_parser.c
source/parser/gemini_parser.c
source/extended_error.c
source/time.c
source/random.c
source/main.c
source/tables.c
source/client.c
source/ring.c
source/processor/gemini_processor.c
source/processor/spartan_processor.c
source/processor/nex_processor.c
source/processor/http_processor.c
source/processor/finger_processor.c
source/processor/gopher_processor.c
source/processor/processor.c
source/value.c
source/memory.c
source/output.c
)

# Define macros
target_compile_definitions(twinkleshine PUBLIC
    TWINKLESHINE_MAJOR=${DEV_MAJOR}
    TWINKLESHINE_MINOR=${DEV_MINOR}
    TWINKLESHINE_PATCH=${DEV_PATCH}
    TWINKLESHINE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:TWINKLESHINE_RELEASE>)
