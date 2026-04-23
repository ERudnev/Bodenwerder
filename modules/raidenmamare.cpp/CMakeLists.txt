file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS
    "source/*.cpp"
    "source/*/*.cpp"
    "source/*/*/*.cpp"
)

project("Raidenmamare"
    DESCRIPTION "Raidenmamare - utilitarian rendering toolkit (graphics engine seed)"
    VERSION 0.1
)

set(Raidenmamare_Q1
    aspects/controller/camera.q1.types
    aspects/controller/dispatcher.q1.types
    aspects/device.q1.types
    aspects/materials/core.q1.types
    aspects/materials/program.q1.types
    aspects/math.q1.types
    aspects/primitives/base.q1.types
    aspects/scene/actor.q1.types
    aspects/scene/camera.q1.types
    aspects/scene/core.q1.types
    aspects/scene/light.q1.types
    aspects/scene/node.q1.types
    aspects/viewport.q1.types
)

add_library(${PROJECT_NAME} STATIC)

add_q1_schema(${PROJECT_NAME}
    ROOT ${CMAKE_CURRENT_SOURCE_DIR}
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
    PUBLIC iQSM
    PUBLIC libglew_static
    PUBLIC glfw
    ${OPENGL_LIBRARIES}
)

