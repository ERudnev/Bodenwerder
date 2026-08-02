add_subdirectory(glew-cmake)
add_subdirectory(glfw)
add_subdirectory(glm)
add_subdirectory(sqlite_amalgamation)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
add_subdirectory(assimp)

# Standalone Boost.PFR (vendored as submodule in 3party/pfr).
# Do not modify or build the submodule itself; just expose headers.
add_library(pfr INTERFACE)
target_include_directories(pfr INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/pfr/include)

add_library(stb_image STATIC stb_image/stb_image.cpp)
target_include_directories(stb_image PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb_image)

add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    imgui
    imgui/backends
)

target_link_libraries(imgui
    PUBLIC
        glfw
)
