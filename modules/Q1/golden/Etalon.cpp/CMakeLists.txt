file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "source/*.cpp")

project("Q1Etalon"
  DESCRIPTION "Etalon.cpp - golden (hand-made) reference for Q1->C++ code generation"
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
  PUBLIC base
  PUBLIC iQSM
  PUBLIC mQSM
)


