file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "*.cpp")

project("render_test"
    DESCRIPTION "render_test - OpenGL 3.3 Hello World triangle demo"
    VERSION 0.1
)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PRIVATE
        ${MODULE_SOURCES}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC glm
    PUBLIC base
    PUBLIC libglew_static
    PUBLIC glfw
    ${OPENGL_LIBRARIES}
)