# The console (engine/console, docs/console.md): cvars and their tiers, the command buffer, aliases,
# scripting, binds and cfg files. No platform and no clock: a frame is one `run`. Simulation code,
# since its cvars set simulation values. Published as ge::console.
file(GLOB GE_CONSOLE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/console/*.cpp)
add_library(ge_console STATIC ${GE_CONSOLE_SOURCES})
target_link_libraries(ge_console PUBLIC ge_core)

ge_module(ge_console EXPORT console SIMULATION)
