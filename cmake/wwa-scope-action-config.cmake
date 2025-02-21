get_filename_component(SCOPE_ACTION_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

list(APPEND CMAKE_MODULE_PATH ${SCOPE_ACTION_CMAKE_DIR})

if(NOT TARGET wwa-scope-action)
    include("${SCOPE_ACTION_CMAKE_DIR}/wwa-scope-action-target.cmake")
    add_library(wwa::scope_action ALIAS wwa-scope-action)
endif()
