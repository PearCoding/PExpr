file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(pexprc ${SRC})
target_link_libraries(pexprc PRIVATE pexpr)
target_include_directories(pexprc SYSTEM PUBLIC ${cli11_SOURCE_DIR}/include)
target_include_directories(pexprc PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
