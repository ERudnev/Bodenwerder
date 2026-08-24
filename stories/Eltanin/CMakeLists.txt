file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "*.cpp")
list(FILTER MODULE_SOURCES EXCLUDE REGEX "[/\\\\]test[/\\\\]")

project("Eltanin"
    DESCRIPTION "Game project. Space"
    VERSION 0.1
)

set(Eltanin_Q1
    world.q1
    physics/body.q1
    physics/compound.q1
    physics/rigid.q1
    entities/block.q1
    resources/geometry.q1
    resources/assets.q1
    mech/semantics.q1
    mech/blueprint.q1
    mech/mount.q1
    geo/minerals.q1
    geo/rock.q1
    geo/boulder.q1
)

add_executable(${PROJECT_NAME})

add_q1_schema(${PROJECT_NAME}
    ROOT ${CMAKE_CURRENT_SOURCE_DIR}/doctrine
    FILES ${Eltanin_Q1}
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

add_executable(Eltanin_horn_tests
    test/horn_orientation.cpp
    private/physics/horn.cpp
)
target_include_directories(Eltanin_horn_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/private
)
target_link_libraries(Eltanin_horn_tests
    PRIVATE
        glm
        base
)
