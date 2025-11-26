CPMAddPackage(
  NAME Catch2
  GITHUB_REPOSITORY catchorg/Catch2 
  GIT_TAG v3.11.0
  EXCLUDE_FROM_ALL YES
  SYSTEM
  OPTIONS
    "CATCH_INSTALL_DOCS OFF"
    "CATCH_INSTALL_EXTRAS OFF"
)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(pexpr_tests ${SRC})
target_link_libraries(pexpr_tests PRIVATE pexpr Catch2WithMain)
add_test(NAME pexpr_tests COMMAND pexpr_tests)
