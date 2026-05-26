#################
# Generate keys #
#################

# Generate keys
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/localhost.key" "${CMAKE_BINARY_DIR}/localhost.crt"
    COMMAND sh -c "yes \"\" | openssl req -x509 -newkey rsa:4096 -sha256 -nodes -subj \"/CN=localhost\" -keyout \"${CMAKE_BINARY_DIR}/localhost.key\" -out \"${CMAKE_BINARY_DIR}/localhost.crt\" 2>/dev/null"
    COMMENT "Generating localhost keys"
    VERBATIM
)

add_custom_target(generate_keys ALL DEPENDS "${CMAKE_BINARY_DIR}/localhost.key" "${CMAKE_BINARY_DIR}/localhost.crt")