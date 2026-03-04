file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "*.cpp")

project("Toy"
    DESCRIPTION "Toy - minimal sample app for DAQL stack (Q1/iQSM/Atomic + rendering playground)"
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
    PRIVATE Raidenmamare
    PRIVATE atomic
    PUBLIC libglew_static
    PUBLIC glfw
    PRIVATE assimp
    ${OPENGL_LIBRARIES}
)