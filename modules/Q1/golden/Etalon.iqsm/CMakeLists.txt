file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "source/*.cpp")

project("Q1_Etalon_iqsm"
  DESCRIPTION "Etalon.iqsm - golden (hand-made) iQSM projection reference for Q1 DSL"
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


