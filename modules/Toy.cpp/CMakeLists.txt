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

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/iQSM.cpp/interface
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
    PUBLIC libglew_static
    PUBLIC glfw
    #PRIVATE assimp
    ${OPENGL_LIBRARIES}
)