# 3.15+ so MSVC gets no default /W3 (policy CMP0092); presets need 3.23 anyway.
cmake_minimum_required(VERSION 3.23)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(DAQL)

# Disable GLFW documentation generation (we don't need Doxygen)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Q1)
include(Warnings)
include(Precompiled)
include(Sanitizers)

add_subdirectory(3party)
add_subdirectory(modules)
add_subdirectory(stories)
#add_subdirectory(workshop)

# Project targets only; 3party keeps its own flags.
set(DAQL_PROJECT_TARGETS
    base base_tests
    fQSM fQSM_tests fQSM_workshop
    pQRF
    Q1_Etalon_fqsm
    Raidenmamare
    Eltanin Eltanin_horn_tests
    Kubes
    TomSawyer
)
daql_enable_warnings(${DAQL_PROJECT_TARGETS})
# Off unless DAQL_SANITIZE is set (see cmake/Sanitizers.cmake and the msvc-asan, clang-ubsan presets).
daql_enable_sanitizers(${DAQL_PROJECT_TARGETS})

# Precompiled headers per project target (see cmake/Precompiled.cmake); 3party untouched.
daql_enable_pch(base            ${DAQL_PCH_STD} ${DAQL_PCH_BASE})
daql_enable_pch(base_tests      ${DAQL_PCH_STD} ${DAQL_PCH_BASE})
daql_enable_pch(fQSM            ${DAQL_PCH_STD} ${DAQL_PCH_BASE} ${DAQL_PCH_FQSM})
daql_enable_pch(fQSM_tests      ${DAQL_PCH_STD} ${DAQL_PCH_BASE} ${DAQL_PCH_FQSM})
daql_enable_pch(fQSM_workshop   ${DAQL_PCH_STD} ${DAQL_PCH_BASE} ${DAQL_PCH_FQSM})
daql_enable_pch(pQRF            ${DAQL_PCH_STD} ${DAQL_PCH_BASE} ${DAQL_PCH_FQSM})
daql_enable_pch(Q1_Etalon_fqsm  ${DAQL_PCH_STD} ${DAQL_PCH_BASE} ${DAQL_PCH_FQSM})
daql_enable_pch(Raidenmamare    ${DAQL_PCH_STD} ${DAQL_PCH_BASE} ${DAQL_PCH_FQSM} ${DAQL_PCH_GLM})
daql_enable_pch(Eltanin         ${DAQL_PCH_STD} ${DAQL_PCH_BASE} ${DAQL_PCH_FQSM} ${DAQL_PCH_GLM})
daql_enable_pch(Kubes           ${DAQL_PCH_STD} ${DAQL_PCH_BASE} ${DAQL_PCH_FQSM} ${DAQL_PCH_GLM})
daql_enable_pch(TomSawyer       ${DAQL_PCH_STD} ${DAQL_PCH_BASE} ${DAQL_PCH_FQSM} ${DAQL_PCH_GLM})
daql_enable_pch(Eltanin_horn_tests ${DAQL_PCH_STD} ${DAQL_PCH_GLM})
