enable_testing()

macro(push_test name files)
	set(TARGET pexpr_test_${name})
	add_executable(${TARGET} ${files})
	target_link_libraries(${TARGET} PRIVATE pexpr)
	add_test(NAME ${name} COMMAND ${TARGET})
endmacro(push_test)

push_test(lexer lexer.cpp)
push_test(parser parser.cpp)