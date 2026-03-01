#######################################
# Creating source packages for Debian #
#######################################

if (UNIX)
    # Create debian_source folder
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/debian_source")

    # Generate control file
    devtemplate_configure_file(twinkleshine_debian_source_control FALSE "${PROJECT_SOURCE_DIR}/config/template/debian_source/control" "${PROJECT_BINARY_DIR}/debian_source/control")

    # Generate changelog file
    devtemplate_configure_file(twinkleshine_debian_source_changelog FALSE "${PROJECT_SOURCE_DIR}/config/template/changelog" "${PROJECT_BINARY_DIR}/debian_source/changelog")

    # Generate copyright file
    devtemplate_configure_file(twinkleshine_debian_source_copyright FALSE "${PROJECT_SOURCE_DIR}/config/template/copyright" "${PROJECT_BINARY_DIR}/debian_source/copyright")

    # Copy rules file
    devtemplate_copy_file(twinkleshine_debian_source_rules FALSE "${PROJECT_SOURCE_DIR}/config/static/debian_source/rules" "${PROJECT_BINARY_DIR}/debian_source/rules")

    # Copy compat file
    devtemplate_copy_file(twinkleshine_debian_source_compat FALSE "${PROJECT_SOURCE_DIR}/config/static/debian_source/compat" "${PROJECT_BINARY_DIR}/debian_source/compat")

    # Copy format file
    devtemplate_copy_file(twinkleshine_debian_source_format FALSE "${PROJECT_SOURCE_DIR}/config/static/debian_source/format" "${PROJECT_BINARY_DIR}/debian_source/format")

    # Define pre-packaging target
    add_custom_target(package_debian_source DEPENDS twinkleshine_debian_source_control twinkleshine_debian_source_changelog twinkleshine_debian_source_copyright twinkleshine_debian_source_rules twinkleshine_debian_source_compat twinkleshine_debian_source_format)
endif()
