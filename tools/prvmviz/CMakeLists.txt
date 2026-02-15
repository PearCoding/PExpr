CPMAddPackage(
    NAME cli11
    GITHUB_REPOSITORY CLIUtils/CLI11
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(prvmviz ${SRC})
target_link_libraries(prvmviz PRIVATE pexpr)
target_include_directories(prvmviz SYSTEM PUBLIC ${cli11_SOURCE_DIR}/include)
target_include_directories(prvmviz PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)