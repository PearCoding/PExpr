cmake_minimum_required(VERSION 3.1...3.20)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# Prevent in source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    if(NOT DEFINED WITH_IN_SOURCE_BUILD)
        message(FATAL_ERROR "CMake generation for PExpr is not allowed within the source directory! Define WITH_IN_SOURCE_BUILD if absolutely necessary!")
    endif()
endif()

# Omit superfluous "Up-to-date" messages.
if(NOT DEFINED CMAKE_INSTALL_MESSAGE)
    set(CMAKE_INSTALL_MESSAGE "LAZY")
endif()

# Set default to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

project(PExpr
    VERSION 0.2
    DESCRIPTION "Inline expressions and statements for mathmatical operations")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(PEXPR_SUBPROJECT OFF)
    set(PEXPR_NOT_SUBPROJECT ON)
else()
    set(PEXPR_SUBPROJECT ON)
    set(PEXPR_NOT_SUBPROJECT OFF)
endif()

if(NOT PEXPR_SUBPROJECT)
    include(CTest)
endif()

# Set corresponding output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_DEBUG_POSTFIX "_d")

message(STATUS "Building PExpr ${PExpr_VERSION}")

option(PEXPR_WITH_ASSERTS "Build with asserts even in release. It is always enabled on debug" OFF)
option(PEXPR_WITH_TOOLS "Build tools" ${PEXPR_NOT_SUBPROJECT})
option(PEXPR_WITH_DOCUMENTATION "Build documentation with doxygen." ${PEXPR_NOT_SUBPROJECT})

# Setup CPM
include(cmake/SetupCPM.cmake)

# Enable folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Add warnings per default
include(CheckCXXCompilerFlag)
set(WARNING_FLAGS -Wall -Wextra -pedantic)
check_cxx_compiler_flag("${WARNING_FLAGS}" COMPILER_SUPPORTS_WARNINGS_GNU)

if(NOT COMPILER_SUPPORTS_WARNINGS_GNU)
    set(WARNING_FLAGS /W4)
    check_cxx_compiler_flag("${WARNING_FLAGS}" COMPILER_SUPPORTS_WARNINGS_MSVC)

    if(NOT COMPILER_SUPPORTS_WARNINGS_MSVC)
        unset(WARNING_FLAGS)
    endif()
endif()

if(WARNING_FLAGS)
    add_compile_options(${WARNING_FLAGS})
endif()

# Enable folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Traverse to underlying directories
add_subdirectory(src)
if(PEXPR_WITH_TOOLS)
    add_subdirectory(tools)
endif()

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

# Documentation
if(PEXPR_WITH_DOCUMENTATION)
    include(cmake/Documentation.cmake)
endif()
