file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "*.cpp")

project("workshop_si01"
    DESCRIPTION "Workshop sample: Q1 Space Invaders experiment (archived, unsupported)"
    VERSION 0.1
)

set(workshop_si01_Q1
    world.q1
    invaders/gameObject.q1
    invaders/session.q1
    invaders/gun.q1
    invaders/actors.q1
    invaders/combat.q1
    invaders/bootstrap.q1
)

add_executable(${PROJECT_NAME})

add_q1_schema(${PROJECT_NAME}
    ROOT ${CMAKE_CURRENT_SOURCE_DIR}/model/doctrine
    FILES ${workshop_si01_Q1}
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
        DAQL_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets" # Unit.library nests under this (workshop/si01)
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
