# The play contract (engine/play: a game's spec and Run, the session and its line protocol, the
# input log, replay and diverge, the bench, the stills, the `play` executable's main), published
# as ge::play so a game's `play` target is driven by `ge play`, `ge sim`, `ge replay`, `ge state`,
# `ge diverge`, `ge shot`, `ge bench`, `ge profile` and `ge mem`. The capture (engine/capture:
# the engine's Matroska muxer, I420, a run exported with its sound) is built into it, since a
# recording (`ge record --format=mkv`) is its user.
file(GLOB GE_CAPTURE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/capture/*.cpp)
add_library(ge_play STATIC engine/play/inputs.cpp
  engine/play/session.cpp engine/play/bench.cpp engine/play/record.cpp engine/play/why.cpp
  engine/play/human.cpp engine/play/devices.cpp engine/play/console_cmds.cpp engine/play/main.cpp engine/play/load.cpp
  ${GE_CAPTURE_SOURCES})
target_link_libraries(ge_play PUBLIC ge_core ge_ai ge_console ge_input ge_lab ge_prof ge_prof_alloc)

# The export's encoders (cmake/Codecs.cmake, GE_ENCODERS) are linked into it privately and only
# in this tree's own build, so the published package and a consumer need neither.
include(cmake/Codecs.cmake)
if(GE_ENCODERS)
  target_link_libraries(ge_play PRIVATE $<BUILD_INTERFACE:ge::aom> $<BUILD_INTERFACE:ge::opus>)
  target_compile_definitions(ge_play PRIVATE GE_ENCODERS=1)
endif()

ge_module(ge_play EXPORT play DIRS play capture)
