# Character movement (engine/move, docs/movement.md): the knobs and presets, one step of a moving
# body over a trace callback, the box world, the strafer and the scenarios. Deterministic, no
# platform; a game binds the knobs to its console and answers the trace with its own world.
file(GLOB GE_MOVE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/move/*.cpp)
add_library(ge_move STATIC ${GE_MOVE_SOURCES})
target_link_libraries(ge_move PUBLIC ge_core ge_console)

ge_module(ge_move EXPORT move SIMULATION)
