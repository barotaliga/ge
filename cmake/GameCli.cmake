# A GAME'S PLAYER CLI (engine/gamecli, docs/gamecli.md, skill: agent-interface): `ge_add_game_cli`
# makes the executable a game's `<game>_cli` runs as, at the build's root (build/<preset>/<name>)
# beside the window and the dedicated server, which it starts from there:
#
#   ge_add_game_cli(<target> OUTPUT_NAME <name> SOURCES <main.cpp...> LINK <the game's libraries...>)
#
# The main is the game's Spec and one call:
#   #include "gamecli/gamecli.hpp"
#   int main(int argc, char** argv) { return ge::gamecli::main(argc, argv, my_cli()); }
include_guard(GLOBAL)
function(ge_add_game_cli target)
  if(NOT TARGET ge_gamecli)
    return()
  endif()
  cmake_parse_arguments(S "" "OUTPUT_NAME" "SOURCES;LINK" ${ARGN})
  foreach(key OUTPUT_NAME SOURCES)
    if(NOT S_${key})
      message(FATAL_ERROR "ge_add_game_cli(${target}): no ${key}; next: pass it "
        "(cmake/GameCli.cmake lists the arguments)")
    endif()
  endforeach()
  add_executable(${target} ${S_SOURCES})
  target_link_libraries(${target} PRIVATE ge_gamecli ${S_LINK})
  if(TARGET ge_flags)
    target_link_libraries(${target} PRIVATE ge_flags)
  endif()
  set_target_properties(${target} PROPERTIES OUTPUT_NAME ${S_OUTPUT_NAME}
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endfunction()
