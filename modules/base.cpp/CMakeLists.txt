file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "source/*.cpp")
file (GLOB_RECURSE TESTING_SOURCES CONFIGURE_DEPENDS "test/*.cpp")

project("base"
    DESCRIPTION "Base - C++ library with set of essential utils"
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
        ${PROJECT_SOURCE_DIR}/interface
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC glm
)

# test target
add_executable(${PROJECT_NAME}_tests)
target_sources(${PROJECT_NAME}_tests
    PRIVATE
        ${TESTING_SOURCES}
)
target_include_directories(${PROJECT_NAME}_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test
)
target_link_libraries(${PROJECT_NAME}_tests
    PRIVATE
        ${PROJECT_NAME}
)

