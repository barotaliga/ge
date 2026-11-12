# A GAME'S HUB BINARY (docs/hub.md, skill: net): `ge_add_hub` makes the executable a game's
# community runs as its website, at the build's root (build/<preset>/<OUTPUT_NAME>), from a main
# that hands the game's HubSpec to the engine's `hub_main` (engine/hub/main.hpp):
#
#   ge_add_hub(<target> OUTPUT_NAME <name> SOURCES <main.cpp...> LINK <the game's libraries...>)
#
# The main is three lines:
#   #include "hub/main.hpp"
#   int main(int argc, char** argv) { return ge::hub::hub_main(argc, argv, my_hub_spec()); }
include_guard(GLOBAL)

function(ge_add_hub target)
  cmake_parse_arguments(H "" "OUTPUT_NAME" "SOURCES;LINK" ${ARGN})
  foreach(key OUTPUT_NAME SOURCES)
    if(NOT H_${key})
      message(FATAL_ERROR "ge_add_hub(${target}): no ${key}; next: pass it (cmake/Hub.cmake "
        "lists the arguments)")
    endif()
  endforeach()
  add_executable(${target} ${H_SOURCES})
  target_link_libraries(${target} PRIVATE ge_hub ${H_LINK})
  set_target_properties(${target} PROPERTIES OUTPUT_NAME ${H_OUTPUT_NAME}
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endfunction()
