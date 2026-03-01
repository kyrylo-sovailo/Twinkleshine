##################
# Install manual #
##################

if (DEV_MAN_NAME)
    # Install manual
    install(FILES "${PROJECT_BINARY_DIR}/${DEV_MAN_NAME}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/man/man${DEV_CATEGORY}")
endif()