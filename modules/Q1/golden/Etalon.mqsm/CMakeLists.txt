file (GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "*.cpp")

project("Q1_Etalon_mqsm"
  DESCRIPTION "Etalon.mqsm - golden (hand-made) mQSM projection reference for Q1 DSL (stubs)"
  VERSION 0.1
)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME}
  PRIVATE
    ${MODULE_SOURCES}
)

target_include_directories(${PROJECT_NAME}
  INTERFACE
    ${PROJECT_SOURCE_DIR}
)
