file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS
    "source/*.cpp"
    "source/*/*.cpp"
    "source/*/*/*.cpp"
)

project("Raidenmamare"
    DESCRIPTION "Raidenmamare - utilitarian rendering toolkit (graphics engine seed)"
    VERSION 0.1
)

add_library(${PROJECT_NAME} STATIC)

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

