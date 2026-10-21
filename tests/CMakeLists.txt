# Every tests/<topic>_test.cpp becomes its own executable and CTest case.
# New test file = new topic; nothing to register by hand.
file(GLOB GE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)
# Every test links the engine and every plugin, so a plugin's tests (tests/<plugin>_*_test.cpp)
# need no wiring either.
get_property(GE_PLUGIN_TARGETS GLOBAL PROPERTY GE_PLUGIN_TARGETS)
foreach(src ${GE_TEST_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} main.cpp)
  target_link_libraries(${name} PRIVATE ge_core ${GE_PLUGIN_TARGETS})
  target_compile_definitions(${name} PRIVATE GE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME ${name} COMMAND ${name})
endforeach()
