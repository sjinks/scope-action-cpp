include(CMakeDependentOption)
option(BUILD_DOCS "Build documentation" ${PROJECT_IS_TOP_LEVEL})
cmake_dependent_option(BUILD_INTERNAL_DOCS "Build internal documentation" OFF "BUILD_DOCS" OFF)

if(BUILD_DOCS)
    include(FindDoxygen)
    find_package(Doxygen)
    if(NOT DOXYGEN_FOUND)
        message(WARNING "Doxygen not found, documentation will not be built")
        set(BUILD_DOCS OFF)
    else()
        if(TARGET Doxygen::dot)
            set(HAVE_DOT "YES")
        else()
            set(HAVE_DOT "NO")
        endif()

        if(BUILD_INTERNAL_DOCS)
            set(ENABLED_SECTIONS "INTERNAL")
            set(EXTRACT_PRIVATE "YES")
        else()
            set(ENABLED_SECTIONS "")
            set(EXTRACT_PRIVATE "NO")
        endif()

        configure_file("${CMAKE_SOURCE_DIR}/cmake/Doxyfile.in" "${CMAKE_BINARY_DIR}/Doxyfile" @ONLY)
        add_custom_target(
            docs
            ALL
            COMMAND Doxygen::doxygen "${CMAKE_BINARY_DIR}/Doxyfile"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_BINARY_DIR}/docs/html/" "${CMAKE_SOURCE_DIR}/apidocs"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
endif()
