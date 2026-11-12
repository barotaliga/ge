# A player's game CLI (engine/gamecli, docs/gamecli.md): the generic commands every game's
# `<game>_cli` answers (settings, servers, players, the remote console, hosting, joining, logs),
# over the console (the player's config, read through the game's catalog), the network (the
# server browser's queries) and the server core (a dedicated server's settings in layers). A
# game's CLI executable is made by `ge_add_game_cli` (cmake/GameCli.cmake). A desktop tool: not
# built where the host tools are off (a mobile target), since it starts programs. Published as
# ge::gamecli, so a game repo makes its own CLI.
if(NOT DEFINED GE_HOST_TOOLS OR GE_HOST_TOOLS)
  file(GLOB GE_GAMECLI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/gamecli/*.cpp)
  add_library(ge_gamecli STATIC ${GE_GAMECLI_SOURCES})
  target_link_libraries(ge_gamecli PUBLIC ge_core ge_console ge_net ge_server ge_forward)
endif()

ge_module(ge_gamecli EXPORT gamecli)
include(cmake/GameCli.cmake)
