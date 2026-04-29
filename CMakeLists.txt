################################################################################
# This project is developed with help of github.com/kyrylo-sovailo/devtemplate #
#                                    Header                                    #
################################################################################
set(DEV_NAME "Twinkleshine")
set(DEV_MAJOR 1)
set(DEV_MINOR 0)
set(DEV_PATCH 0)
set(DEV_VERSION ${DEV_MAJOR}.${DEV_MINOR}.${DEV_PATCH})
set(DEV_TYPE SHARED)
set(DEV_CATEGORY 1)
set(DEV_HOMEPAGE "https://github.com/kyrylo-sovailo/twinkleshine")
set(DEV_EMAIL "k.sovailo@gmail.com")
set(DEV_AUTHOR "Kyrylo Sovailo")
set(DEV_DESCRIPTION "Multi-protocol server")
set(DEV_EXTENDED_DESCRIPTION "Standalone server that can handle multiple exotic protocols, all in one process: HTTP, HTTPS, Gopher, Gemini")

cmake_minimum_required(VERSION 3.13)
project(twinkleshine VERSION ${DEV_VERSION} LANGUAGES C)
include(config/cmake/common.cmake)

####################
# Generic features #
####################
include(config/cmake/executable.cmake)
include(config/cmake/manual.cmake)

###############################
# Installing generic features #
###############################
include(config/cmake/install-executable.cmake)
include(config/cmake/install-manual.cmake)

configure_file("${PROJECT_SOURCE_DIR}/config/template/environment.sh" "${PROJECT_BINARY_DIR}//environment.sh")

#############
# Packaging #
#############
add_custom_target(core DEPENDS ${DEV_CORE_TARGETS})
include(config/cmake/package-debian-binary.cmake)
include(config/cmake/package-debian-source.cmake)
