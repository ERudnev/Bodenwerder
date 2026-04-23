file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "*.cpp")

project("Q1_Etalon_mqsm"
  DESCRIPTION "Etalon.mqsm - golden (hand-made) mQSM projection reference for Q1 DSL (stubs)"
  VERSION 0.1
)

set(Q1_Etalon_mqsm_Q1
  aspects.q1.types
)

add_library(${PROJECT_NAME} STATIC)

add_q1_schema(${PROJECT_NAME}
  ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../Etalon.q1
  FILES ${Q1_Etalon_mqsm_Q1}
)

target_sources(${PROJECT_NAME}
  PRIVATE
    ${MODULE_SOURCES}
)

target_include_directories(${PROJECT_NAME}
  INTERFACE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC mQSM
)
