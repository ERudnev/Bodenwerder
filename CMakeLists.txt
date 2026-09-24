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

add_subdirectory(3party)
add_subdirectory(modules)
add_subdirectory(stories)
#add_subdirectory(workshop)

# Project targets only; 3party keeps its own flags.
daql_enable_warnings(
    base base_tests
    fQSM fQSM_tests fQSM_workshop
    pQRF
    Q1_Etalon_fqsm
    Raidenmamare
    Eltanin Eltanin_horn_tests
    Kubes
    TomSawyer
)

