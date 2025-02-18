if(ENABLE_MAINTAINER_MODE AND (CMAKE_COMPILER_IS_GNU OR CMAKE_COMPILER_IS_CLANG))
    string(REPLACE " " ";" COMPILE_OPTIONS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MM} -Wno-global-constructors -Wno-exit-time-destructors -Wno-weak-vtables -Wno-disabled-macro-expansion")
    set_directory_properties(PROPERTIES COMPILE_OPTIONS "${COMPILE_OPTIONS}")
    unset(COMPILE_OPTIONS)
endif()

set_directory_properties(PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/src")

set(TEST_TARGET test_scope_action)

add_executable(
    "${TEST_TARGET}"
    exit_action.cpp
    fail_action.cpp
    success_action.cpp
)

target_link_libraries("${TEST_TARGET}" PRIVATE ${PROJECT_NAME} GTest::gtest_main)
set_target_properties(
    "${TEST_TARGET}"
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

if(NOT CMAKE_CROSSCOMPILING)
    include(GoogleTest)
    gtest_discover_tests("${TEST_TARGET}")
endif()

if(ENABLE_COVERAGE)
    add_dependencies("${TEST_TARGET}" clean_coverage)
    add_dependencies(generate_coverage "${TEST_TARGET}")
endif()
