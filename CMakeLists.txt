cmake_minimum_required(VERSION 3.1...3.20)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# Prevent in source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	if(NOT DEFINED WITH_IN_SOURCE_BUILD)
		messag(FATAL_ERROR "CMake generation for pexpr is not allowed within the source directory! Define WITH_IN_SOURCE_BUILD if absolutely necessary!" )
	endif()
endif()

# Omit superfluous "Up-to-date" messages.
if(NOT DEFINED CMAKE_INSTALL_MESSAGE)
	set(CMAKE_INSTALL_MESSAGE "LAZY")
endif()

# Set default to Release
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

project(pexpr
	VERSION 0.1
	DESCRIPTION "Simple inline expressions for mathmatical operations")

# For whatever reason Ignis might be used as a subproject...
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	set(PEXPR_SUBPROJECT OFF)
	set(PEXPR_NOT_SUBPROJECT ON)
else()
	set(PEXPR_SUBPROJECT ON)
	set(PEXPR_NOT_SUBPROJECT OFF)
endif()

# Set corresponding output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_DEBUG_POSTFIX  "_d")

message(STATUS "Building pexpr ${pexpr_VERSION}")

option(PEXPR_WITH_ASSERTS "Build with asserts even in release. It is always enabled on debug" OFF)
option(PEXPR_WITH_TESTS "Build tests" PEXPR_NOT_SUBPROJECT)

# Enable folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Add warnings per default
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

# Traverse to underlying directories
add_subdirectory(src)

if(PEXPR_WITH_TESTS)
    include(CTest)
	add_subdirectory(test)
endif()