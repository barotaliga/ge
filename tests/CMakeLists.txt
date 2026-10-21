# Every tests/<topic>_test.cpp becomes its own executable and CTest case.
# New test file = new topic; nothing to register by hand.
file(GLOB GE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)
# Every test links the engine, every plugin and every game's simulation, so a plugin's tests
# (tests/<plugin>_*_test.cpp) need no wiring either.
get_property(GE_PLUGIN_TARGETS GLOBAL PROPERTY GE_PLUGIN_TARGETS)
get_property(GE_GAME_TARGETS GLOBAL PROPERTY GE_GAME_TARGETS)
# tests/cli_*_test.cpp need GE_CLI and link ge_cli (the ge command line).
# tests/gpu_*_test.cpp need GE_GPU and link ge_gpu; tests/platform_*_test.cpp need GE_PLATFORM
# and GE_GPU (a window is only worth opening to present to). Either kind exits 77 (CTest's
# SKIP) when the machine cannot run it (no adapter, no display), except where the environment
# sets GE_GPU_REQUIRE=1 (CI's Linux x64 rows, with lavapipe and a virtual display): there a
# skip is a failure.
foreach(src ${GE_TEST_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  set(extra)
  set(cli)
  if(name MATCHES "^gpu_")
    if(NOT GE_GPU)
      continue()
    endif()
    set(extra ge_gpu)
  elseif(name MATCHES "^cli_")
    if(NOT GE_CLI)
      continue()
    endif()
    set(cli ge_cli)
  elseif(name MATCHES "^platform_")
    if(NOT GE_PLATFORM OR NOT GE_GPU)
      continue()
    endif()
    set(extra ge_platform ge_gpu)
  endif()
  add_executable(${name} ${src} main.cpp)
  target_link_libraries(${name} PRIVATE ge_core ge_lab ${GE_PLUGIN_TARGETS} ${GE_GAME_TARGETS}
    ${extra} ${cli})
  target_compile_definitions(${name} PRIVATE GE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME ${name} COMMAND ${name})
  if(cli)
    # A machine that cannot make symlinks (Windows without Developer Mode) cannot hold a game
    # repo's guidance; the scaffold's tests skip there.
    set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
  endif()
  if(extra)
    # The software driver's worker threads keep allocations past exit, which the leak check
    # would report as ours; every other test keeps it.
    set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77
      ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
  endif()
endforeach()

# Every lab at its smoke flags, twice in one process (a digest that moves between the two runs
# is a determinism break), and each digest against the committed baseline, which the same code
# must reproduce on every compiler row (docs/labs.md § Digests; `make labs-digests` rewrites it).
add_test(NAME labs_smoke COMMAND labs --all --smoke --repeat=2 --quiet
  --out=${PROJECT_BINARY_DIR}/previews
  --check-digests=${PROJECT_SOURCE_DIR}/tests/labs_digests.txt)
# From the repository's root, where a lab finds its golden images (gpu-capture). With the GPU
# in, the software driver's worker threads keep allocations past exit (as in the gpu_ tests).
set_tests_properties(labs_smoke PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
if(GE_GPU)
  set_tests_properties(labs_smoke PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()
