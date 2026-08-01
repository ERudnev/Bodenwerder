file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "*.cpp")

project("Kubes"
    DESCRIPTION "Succeding former Toy project"
    VERSION 0.1
)

set(Kubes_Q1
    world.q1.types
    resources/geometry.q1.types
)

add_executable(${PROJECT_NAME})

add_q1_schema(${PROJECT_NAME}
    ROOT ${CMAKE_CURRENT_SOURCE_DIR}/doctrine
    FILES ${Kubes_Q1}
)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${MODULE_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/model
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        DAQL_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC glm
    PUBLIC base
    PRIVATE fQSM
    PRIVATE Raidenmamare
    PRIVATE pQRF
    PRIVATE imgui
    PUBLIC libglew_static
    PUBLIC glfw
    #PRIVATE assimp
    ${OPENGL_LIBRARIES}
)