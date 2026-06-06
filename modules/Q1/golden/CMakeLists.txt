file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "Etalon.fqsm/*.cpp")

project("Q1_Etalon_fqsm"
  DESCRIPTION "Etalon.fqsm - golden (hand-made) fQSM projection reference for Q1 DSL"
  VERSION 0.1
)

set(Q1_Etalon_fqsm_Q1
  aspects.q1.types
  elementary.q1.types
  resource.q1.types
)

add_library(${PROJECT_NAME} STATIC ${MODULE_SOURCES})

add_q1_schema(${PROJECT_NAME}
  ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Etalon.q1
  FILES ${Q1_Etalon_fqsm_Q1}
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Etalon.fqsm
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC fQSM
)


