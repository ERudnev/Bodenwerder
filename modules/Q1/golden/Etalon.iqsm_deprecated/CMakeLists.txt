file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "source/*.cpp")

project("Q1_Etalon_iqsm"
  DESCRIPTION "Etalon.iqsm - golden (hand-made) iQSM projection reference for Q1 DSL"
  VERSION 0.1
)

set(Q1_Etalon_iqsm_Q1
  aspects.q1.types
  elementary.q1.types
  resource.q1.types
)

add_library(${PROJECT_NAME} STATIC)

add_q1_schema(${PROJECT_NAME}
  ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../Etalon.q1
  FILES ${Q1_Etalon_iqsm_Q1}
)

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
  PUBLIC iQSM
)


