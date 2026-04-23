cmake_minimum_required(VERSION 3.10)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(DAQL)

# Disable GLFW documentation generation (we don't need Doxygen)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Q1)

add_subdirectory(3party)
add_subdirectory(modules)

