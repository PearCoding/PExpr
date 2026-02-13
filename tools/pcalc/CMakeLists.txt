CPMAddPackage(
    NAME cli11
    GITHUB_REPOSITORY CLIUtils/CLI11
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(pcalc ${SRC})
target_link_libraries(pcalc PRIVATE pexpr)
target_include_directories(pcalc SYSTEM PUBLIC ${cli11_SOURCE_DIR}/include)
target_include_directories(pcalc PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
