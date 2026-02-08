file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

# Fuzzy testing executable
add_executable(pexpr_fuzzy_tests ${SRC})

# Link with Catch2 and PExpr libraries
target_link_libraries(pexpr_fuzzy_tests PRIVATE pexpr)

# Make the executable discoverable by ctest (optional)
# add_test(NAME pexpr_fuzzy_tests COMMAND pexpr_fuzzy_tests)