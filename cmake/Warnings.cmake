# Warning flags for project targets only. Vendored code under 3party/ is not touched.
# Usage: daql_enable_warnings(target1 target2 ...) ; unknown target names are skipped.

function(daql_enable_warnings)
    foreach(DAQL_TARGET IN LISTS ARGN)
        if(NOT TARGET ${DAQL_TARGET})
            message(STATUS "Warnings: target '${DAQL_TARGET}' not found, skipped")
            continue()
        endif()
        get_target_property(DAQL_TARGET_TYPE ${DAQL_TARGET} TYPE)
        if(DAQL_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
            continue()
        endif()
        # Select by frontend, not by compiler id: clang-cl takes cl-style flags and turns
        # a GNU-style -Wall into -Weverything.
        # /bigobj: heavy template TUs exceed the COFF section limit (C1128) without it.
        if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_compile_options(${DAQL_TARGET} PRIVATE /W4 /bigobj)
        else()
            target_compile_options(${DAQL_TARGET} PRIVATE -Wall -Wextra)
        endif()
    endforeach()
endfunction()
