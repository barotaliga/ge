# Every tests/<topic>_test.cpp becomes its own executable and CTest case.
# New test file = new topic; nothing to register by hand.
file(GLOB GE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)
# Every test links the engine and every plugin, so a plugin's tests (tests/<plugin>_*_test.cpp)
# need no wiring either.
get_property(GE_PLUGIN_TARGETS GLOBAL PROPERTY GE_PLUGIN_TARGETS)
foreach(src ${GE_TEST_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} main.cpp)
  target_link_libraries(${name} PRIVATE ge_core ge_lab ${GE_PLUGIN_TARGETS})
  target_compile_definitions(${name} PRIVATE GE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME ${name} COMMAND ${name})
endforeach()

# Every lab at its smoke flags, twice in one process (a digest that moves between the two runs
# is a determinism break), and each digest against the committed baseline, which the same code
# must reproduce on every compiler row (docs/labs.md § Digests; `make labs-digests` rewrites it).
add_test(NAME labs_smoke COMMAND labs --all --smoke --repeat=2 --quiet
  --out=${PROJECT_BINARY_DIR}/previews
  --check-digests=${PROJECT_SOURCE_DIR}/tests/labs_digests.txt)
