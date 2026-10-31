# A GAME'S DEDICATED SERVER BINARY (docs/servers.md, skill: net): `ge_add_server` makes the
# executable a game's server runs as, at the build's root (build/<preset>/<OUTPUT_NAME>), from a
# main that hands the game's ServerGame to the engine's `server_main` (engine/server/main.hpp):
#
#   ge_add_server(<target> OUTPUT_NAME <name> SOURCES <main.cpp...> LINK <the game's libraries...>)
#
# The main is three lines:
#   #include "server/main.hpp"
#   int main(int argc, char** argv) { return ge::server::server_main(argc, argv, my_game()); }
include_guard(GLOBAL)
function(ge_add_server target)
  cmake_parse_arguments(S "" "OUTPUT_NAME" "SOURCES;LINK" ${ARGN})
  foreach(key OUTPUT_NAME SOURCES)
    if(NOT S_${key})
      message(FATAL_ERROR "ge_add_server(${target}): no ${key}; next: pass it (cmake/Server.cmake "
        "lists the arguments)")
    endif()
  endforeach()
  add_executable(${target} ${S_SOURCES})
  target_link_libraries(${target} PRIVATE ge_server ${S_LINK})
  set_target_properties(${target} PROPERTIES OUTPUT_NAME ${S_OUTPUT_NAME}
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endfunction()
