file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_library(pexpr STATIC ${SRC})
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(pexpr PUBLIC Threads::Threads)
endif()
target_include_directories(pexpr PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
target_compile_definitions(pexpr PUBLIC "$<$<CONFIG:Debug>:PEXPR_DEBUG>")
if(PEXPR_WITH_ASSERTS)
    target_compile_definitions(pexpr PUBLIC "PEXPR_WITH_ASSERTS")
endif()
target_compile_features(pexpr PUBLIC cxx_std_20)
set_target_properties(pexpr PROPERTIES POSITION_INDEPENDENT_CODE ON)

install(TARGETS pexpr
    LIBRARY DESTINATION lib
)
install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/src/
    DESTINATION include/PExpr
    FILES_MATCHING PATTERN "*.h*")
