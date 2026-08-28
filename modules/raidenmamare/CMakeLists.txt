file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "source/*.cpp")

project("Raidenmamare"
    DESCRIPTION "Raidenmamare - utilitarian rendering toolkit (graphics engine seed)"
    VERSION 0.1
)

set(Raidenmamare_Q1
    math.q1
    system/core.q1
    system/interface.q1
    system/imgui.q1
    system/window.q1
    system/viewport.q1
    scene/root.q1
    scene/node.q1
    scene/camera.q1
    scene/light.q1
    scene/actors/mesh.q1
    scene/actors/sprite.q1
    scene/actors/family.q1
    scene/gizmos.q1
    resources/manager.q1
    semantics.q1
    resources/textures.q1
    resources/texpack.q1
    resources/texture3array.q1
    resources/shaders.q1
    resources/materials.q1
    resources/overlays.q1
    resources/shadows.q1
    resources/geometry.q1
    resources/meshpack.q1
    resources/sprites.q1
    resources/runtimes.q1
    controllers/camera3d.q1
    controllers/camera2d.q1
    controllers/cameraOrbit.q1
    renderer/gl.q1
    renderer/types.q1
)

add_library(${PROJECT_NAME} STATIC)

add_q1_schema(${PROJECT_NAME}
    ROOT ${CMAKE_CURRENT_SOURCE_DIR}/doctrine
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
    PRIVATE assimp
    ${OPENGL_LIBRARIES}
)
