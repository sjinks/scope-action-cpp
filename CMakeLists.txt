cmake_minimum_required(VERSION 3.25.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

project(
    wwa-scope-action
    VERSION 1.0.0
    DESCRIPTION "Scope guard utilities for managing exit actions in C++"
    HOMEPAGE_URL "https://sjinks.github.io/scope-action-cpp/"
    LANGUAGES CXX
)

option(BUILD_TESTING "Whether to enable tests" ${PROJECT_IS_TOP_LEVEL})
option(BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(INSTALL_SCOPE_ACTION "Whether to enable install targets" ${PROJECT_IS_TOP_LEVEL})
option(ENABLE_MAINTAINER_MODE "Enable maintainer mode" OFF)
option(USE_CLANG_TIDY "Use clang-tidy" OFF)

include(build_types)
include(tools)
include(coverage)
include(docs)

add_subdirectory(src)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTING)
    include(FindGTest)
    find_package(GTest CONFIG)
    if(NOT TARGET GTest::gtest)
        message(STATUS "Google Test not found, fetching it from GitHub")
        # renovate: datasource=github-tags depName=google/googletest
        set(GTEST_VERSION "v1.17.0")
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest
            GIT_TAG "${GTEST_VERSION}"
            GIT_SHALLOW ON
        )

        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    include(CTest)
    enable_testing()
    add_subdirectory(test)
endif()
