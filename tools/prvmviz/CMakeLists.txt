include(CheckCXXCompilerFlag)

# Enable UTF-8 on windows
set(UTF8_FLAGS /utf-8)
check_cxx_compiler_flag("${UTF8_FLAGS}" COMPILER_SUPPORTS_UTF8_FLAGS)

if(NOT COMPILER_SUPPORTS_UTF8_FLAGS)
    # Clang and GCC use UTF-8 by default
    unset(UTF8_FLAGS)
endif()

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(prvmviz ${SRC})
target_link_libraries(prvmviz PRIVATE pexpr)
target_include_directories(prvmviz SYSTEM PUBLIC ${cli11_SOURCE_DIR}/include)
target_include_directories(prvmviz PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_options(prvmviz PRIVATE ${UTF8_FLAGS})
