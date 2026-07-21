file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "*.cpp")

project("pQRF"
    DESCRIPTION "C++ implementation of 'perstent Quantized Relational Form'"
    VERSION 0.1
)

# main target
add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
    PRIVATE
        ${MODULE_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/..
)
target_link_libraries(${PROJECT_NAME}
    PUBLIC base
    PUBLIC pfr
    PUBLIC fQSM
    PUBLIC sqlite3
)