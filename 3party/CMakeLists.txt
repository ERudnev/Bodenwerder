add_subdirectory(glew-cmake)
add_subdirectory(glfw)
add_subdirectory(glm)
#add_subdirectory(assimp)

# Standalone Boost.PFR (vendored as submodule in 3party/pfr).
# Do not modify or build the submodule itself; just expose headers.
add_library(pfr INTERFACE)
target_include_directories(pfr INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/pfr/include)
