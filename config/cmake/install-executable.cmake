###################
# Install targets #
###################

# Install targets
install(TARGETS ${DEV_EXPORT_TARGETS}
    EXPORT twinkleshine_export
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/twinkleshine")
set(twinkleshine_export TRUE)