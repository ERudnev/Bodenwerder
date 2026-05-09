file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "source/*.cpp")
file (GLOB_RECURSE TESTING_SOURCES CONFIGURE_DEPENDS "test/*.cpp")

project("iQSM"
    DESCRIPTION "iqsm.cpp - C++ implementation of iQSM"
    VERSION 0.1
)

# main target 
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
    PUBLIC base
    PUBLIC pfr
    ${OPENGL_LIBRARIES}
)

# test target
add_executable(${PROJECT_NAME}_tests)
target_sources(${PROJECT_NAME}_tests
    PRIVATE
        ${TESTING_SOURCES}
)
target_link_libraries(${PROJECT_NAME}_tests
    PRIVATE
        ${PROJECT_NAME}
        #Q1_Etalon_iqsm
)

