# The toolkit (engine/toolkit, docs/gamecli.md § The toolkit): the bodies of the commands a modder
# and a player share, written once and run by both front doors: `ge` (engine/cli) and every
# game's player CLI (engine/gamecli, `<game>_cli`). Each command group is rows of data and a run
# function over a call (toolkit/call.hpp) that answers an outcome (toolkit/outcome.hpp). No
# process, no platform: archives, files, the engine's readers and the mod loader. Published as ge::toolkit, so a
# game repo's CLI links the same groups.
file(GLOB GE_TOOLKIT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/toolkit/*.cpp)
add_library(ge_toolkit STATIC ${GE_TOOLKIT_SOURCES})
target_link_libraries(ge_toolkit PUBLIC ge_core ge_vfs ge_assets ge_mod ge_net)

ge_module(ge_toolkit EXPORT toolkit)
