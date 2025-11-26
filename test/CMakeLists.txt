enable_testing()

add_subdirectory(unittests)

add_executable(pexpr_checker checker.cpp)
target_link_libraries(pexpr_checker PRIVATE pexpr)

macro(push_check name file)
    if(PEXPR_SUBPROJECT)
        string(CONCAT test_name "pexpr_" ${name})
    else()
        set(test_name ${name})
    endif()

    add_test(NAME ${test_name} COMMAND pexpr_checker -f ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endmacro(push_check)

macro(push_check2 name)
    push_check(${name} ${name}.pexpr)
endmacro()

push_check2(comments)
push_check2(variables)
push_check2(functions)
push_check2(closures)
