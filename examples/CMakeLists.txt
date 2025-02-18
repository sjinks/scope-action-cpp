if(ENABLE_MAINTAINER_MODE AND (CMAKE_COMPILER_IS_GNU OR CMAKE_COMPILER_IS_CLANG))
    string(REPLACE " " ";" COMPILE_OPTIONS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MM}")
    set_directory_properties(PROPERTIES COMPILE_OPTIONS "${COMPILE_OPTIONS}")
    unset(COMPILE_OPTIONS)
endif()

set_directory_properties(PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/src")

add_executable(scope_action scope_action.cpp)
target_link_libraries(scope_action PRIVATE wwa::scope_action)
