set(PUBLIC
    pexpr_config.h
    PExpr.h
    Definitions.h
    Enums.cpp
    Enums.h
    Environment.cpp
    Environment.h
    Expression.h
    Location.h
    Logger.h
    LogListener.h
    StringVisitor.h
)

set(SRC
    ${PUBLIC}
    Logger.cpp

    internal/ConsoleLogListener.cpp
    internal/ConsoleLogListener.h
    internal/DefContainer.h
    internal/Lexer.cpp
    internal/Lexer.h
    internal/Parser.cpp
    internal/Parser.h
    internal/Token.cpp
    internal/Token.h
    internal/TypeChecker.cpp
    internal/TypeChecker.h
)

add_library(pexpr ${SRC})
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(pexpr PUBLIC Threads::Threads)
endif()
target_include_directories(pexpr PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
target_compile_definitions(pexpr PUBLIC "$<$<CONFIG:Debug>:PEXPR_DEBUG>")
if(PEXPR_WITH_ASSERTS)
  target_compile_definitions(pexpr PUBLIC "PEXPR_WITH_ASSERTS")
endif()
target_compile_features(pexpr PUBLIC cxx_std_17)
set_target_properties(pexpr PROPERTIES POSITION_INDEPENDENT_CODE ON)