# Every tests/<topic>_test.cpp becomes its own executable and CTest case.
# New test file = new topic; nothing to register by hand.
file(GLOB GE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)
foreach(src ${GE_TEST_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} main.cpp)
  target_link_libraries(${name} PRIVATE ge_core)
  target_compile_definitions(${name} PRIVATE GE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME ${name} COMMAND ${name})
endforeach()
