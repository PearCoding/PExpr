CPMAddPackage(
    NAME cli11
    GITHUB_REPOSITORY CLIUtils/CLI11
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

add_subdirectory(pexprc)
add_subdirectory(pcalc)
add_subdirectory(prvmviz)
