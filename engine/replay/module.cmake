# Replays (engine/replay, docs/replays.md): the keyframes a seek starts from, the player that steps
# a run off its input log to any step, the transport; over the play contract's Run. Published as
# ge::replay.
file(GLOB GE_REPLAY_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/replay/*.cpp)
add_library(ge_replay STATIC ${GE_REPLAY_SOURCES})
target_link_libraries(ge_replay PUBLIC ge_core ge_play)

ge_module(ge_replay EXPORT replay)
