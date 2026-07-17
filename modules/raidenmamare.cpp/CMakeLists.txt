file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "source/*.cpp")

project("Raidenmamare"
    DESCRIPTION "Raidenmamare - utilitarian rendering toolkit (graphics engine seed)"
    VERSION 0.1
)

set(Raidenmamare_Q1
    math.q1.types
    system/core.q1.types
    system/interface.q1.types
    system/imgui.q1.types
    system/window.q1.types
    system/viewport.q1.types
    scene/root.q1.types
    scene/node.q1.types
    scene/camera.q1.types
    scene/light.q1.types
    scene/actor.q1.types
    scene/gizmos.q1.types
    resources/manager.q1.types
    resources/semantics.q1.types
    resources/textures.q1.types
    resources/shaders.q1.types
    resources/materials.q1.types
    resources/shadows.q1.types
    resources/geometry.q1.types
    resources/runtimes.q1.types
    controllers/camera.q1.types
    renderer/gl.q1.types
    renderer/types.q1.types
)

add_library(${PROJECT_NAME} STATIC)

add_q1_schema(${PROJECT_NAME}
    ROOT ${CMAKE_CURRENT_SOURCE_DIR}/declaration
    FILES ${Raidenmamare_Q1}
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${MODULE_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
        ${CMAKE_CURRENT_SOURCE_DIR}/interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/interface
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC fQSM
    PUBLIC libglew_static
    PUBLIC glfw
    PUBLIC stb_image
    PRIVATE imgui
    ${OPENGL_LIBRARIES}
)
