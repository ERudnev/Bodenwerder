# Sanitizer instrumentation for project targets only. Vendored code under 3party/ is not touched.
# Configure with -DDAQL_SANITIZE=<list>, where <list> is a comma separated subset of:
#   address    AddressSanitizer:            cl (preset msvc-asan), gcc, clang
#   undefined  UndefinedBehaviorSanitizer:  clang-cl (preset clang-ubsan), gcc, clang
# Usage: daql_enable_sanitizers(target1 target2 ...) ; unknown target names are skipped.
#
# MSVC frontend notes (verified 2026-09-25 with MSVC 14.51 and clang-cl 21 in a vcvars64 shell):
# - cl has no UndefinedBehaviorSanitizer.
# - clang-cl with -fsanitize=address miscompiles C++ exception handling here: the smallest
#   try/catch crashes inside the catch handler, and a catch that frees and rethrows frees twice.
#   That holds for the toolset ASan runtime and for the LLVM one, so address is refused on clang-cl
#   and its false reports are not mistaken for engine bugs. ASan on this frontend is cl only.
# - cl ASan objects carry /INFERASANLIBS: link.exe adds the toolset ASan runtime by itself.
#   /RTC1 (CMake default for Debug) and /INCREMENTAL are incompatible with it; both are removed.
#   The runtime DLL clang_rt.asan_dynamic-x86_64.dll is on PATH in a vcvars shell and is also copied
#   next to each instrumented executable at configure time, so it runs from any shell.
# - clang-cl UBSan objects name the standalone UBSan runtime in a /DEFAULTLIB directive; the linker
#   only needs the LLVM runtime library directory. That runtime is built for the static release CRT,
#   so the whole clang-cl UBSan build uses /MT (3party too, the CRT must be one across the link).
#   Debug still keeps assert() on (no NDEBUG). CMake pairs clang-cl with lld-link; link.exe is
#   forced, which is the linker the recipe was verified with.
#
# Run time: ASAN_OPTIONS and UBSAN_OPTIONS take the usual sanitizer flags. UBSan reports and
# continues by default; UBSAN_OPTIONS=halt_on_error=1 makes the first report fatal, and
# print_stacktrace=1 adds a stack to each report.

set(DAQL_SANITIZE "" CACHE STRING "Sanitizers for project targets: address, undefined or address,undefined (empty = off)")

if(DAQL_SANITIZE)
    string(REPLACE "," ";" DAQL_SANITIZE_LIST "${DAQL_SANITIZE}")
    foreach(DAQL_SANITIZER IN LISTS DAQL_SANITIZE_LIST)
        if(NOT DAQL_SANITIZER MATCHES "^(address|undefined)$")
            message(FATAL_ERROR "DAQL_SANITIZE: unknown sanitizer '${DAQL_SANITIZER}' (known: address, undefined)")
        endif()
    endforeach()
    string(REPLACE ";" "," DAQL_SANITIZE_SPEC "${DAQL_SANITIZE_LIST}")

    if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            if("address" IN_LIST DAQL_SANITIZE_LIST)
                message(FATAL_ERROR "DAQL_SANITIZE: clang-cl ASan breaks C++ exception handling (see cmake/Sanitizers.cmake); use the msvc-asan preset for address")
            endif()
            execute_process(COMMAND "${CMAKE_CXX_COMPILER}" /clang:-print-resource-dir
                OUTPUT_VARIABLE DAQL_CLANG_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE DAQL_CLANG_RESOURCE_RESULT)
            file(TO_CMAKE_PATH "${DAQL_CLANG_RESOURCE_DIR}" DAQL_CLANG_RESOURCE_DIR)
            set(DAQL_UBSAN_LIB_DIR "${DAQL_CLANG_RESOURCE_DIR}/lib/windows")
            if(NOT DAQL_CLANG_RESOURCE_RESULT EQUAL 0 OR NOT EXISTS "${DAQL_UBSAN_LIB_DIR}/clang_rt.ubsan_standalone-x86_64.lib")
                message(FATAL_ERROR "DAQL_SANITIZE: clang_rt.ubsan_standalone-x86_64.lib not found under '${DAQL_UBSAN_LIB_DIR}'")
            endif()
            set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
            set(CMAKE_LINKER_TYPE MSVC)
        else()
            if("undefined" IN_LIST DAQL_SANITIZE_LIST)
                message(FATAL_ERROR "DAQL_SANITIZE: cl has no UndefinedBehaviorSanitizer; use the clang-ubsan preset for undefined")
            endif()
            # Directory-scope copies of the cache flags; every add_subdirectory below inherits them.
            foreach(DAQL_FLAGS IN ITEMS CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
                string(REGEX REPLACE "[/-]RTC[1csu]+" "" ${DAQL_FLAGS} "${${DAQL_FLAGS}}")
            endforeach()
            foreach(DAQL_FLAGS IN ITEMS CMAKE_EXE_LINKER_FLAGS_DEBUG CMAKE_SHARED_LINKER_FLAGS_DEBUG)
                string(REGEX REPLACE "[/-]INCREMENTAL(:[A-Za-z]+)?" "" ${DAQL_FLAGS} "${${DAQL_FLAGS}}")
                set(${DAQL_FLAGS} "${${DAQL_FLAGS}} /INCREMENTAL:NO")
            endforeach()
            if(DEFINED ENV{VCToolsInstallDir})
                file(TO_CMAKE_PATH "$ENV{VCToolsInstallDir}" DAQL_VC_TOOLS)
                set(DAQL_ASAN_DLL "${DAQL_VC_TOOLS}/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll")
                if(NOT EXISTS "${DAQL_ASAN_DLL}")
                    unset(DAQL_ASAN_DLL)
                endif()
            endif()
        endif()
    endif()
    message(STATUS "Sanitizers: ${DAQL_SANITIZE_SPEC} on project targets")
endif()

function(daql_enable_sanitizers)
    if(NOT DAQL_SANITIZE)
        return()
    endif()
    foreach(DAQL_TARGET IN LISTS ARGN)
        if(NOT TARGET ${DAQL_TARGET})
            message(STATUS "Sanitizers: target '${DAQL_TARGET}' not found, skipped")
            continue()
        endif()
        get_target_property(DAQL_TARGET_TYPE ${DAQL_TARGET} TYPE)
        if(DAQL_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
            continue()
        endif()
        if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                target_compile_options(${DAQL_TARGET} PRIVATE -fsanitize=${DAQL_SANITIZE_SPEC} /Oy-)
                if(DAQL_TARGET_TYPE STREQUAL "EXECUTABLE")
                    target_link_directories(${DAQL_TARGET} PRIVATE "${DAQL_UBSAN_LIB_DIR}")
                endif()
            else()
                target_compile_options(${DAQL_TARGET} PRIVATE /fsanitize=address)
                if(DAQL_TARGET_TYPE STREQUAL "EXECUTABLE" AND DAQL_ASAN_DLL)
                    # Configure-time copy: a POST_BUILD step may only be added from the directory that
                    # created the target, and the executables set no RUNTIME_OUTPUT_DIRECTORY of their own.
                    get_target_property(DAQL_RUNTIME_DIR ${DAQL_TARGET} RUNTIME_OUTPUT_DIRECTORY)
                    if(NOT DAQL_RUNTIME_DIR)
                        get_target_property(DAQL_RUNTIME_DIR ${DAQL_TARGET} BINARY_DIR)
                    endif()
                    file(COPY "${DAQL_ASAN_DLL}" DESTINATION "${DAQL_RUNTIME_DIR}")
                endif()
            endif()
        else()
            target_compile_options(${DAQL_TARGET} PRIVATE -fsanitize=${DAQL_SANITIZE_SPEC} -fno-omit-frame-pointer)
            target_link_options(${DAQL_TARGET} PRIVATE -fsanitize=${DAQL_SANITIZE_SPEC})
        endif()
    endforeach()
endfunction()
