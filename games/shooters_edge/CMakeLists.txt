# Shooter's Edge, the engine's second game (docs/shooters-edge.md). Its simulation (sim/) is one
# static library the labs and the tests link: the native game program, the test arena and the
# scripted match over plugins/shooter's classic loop (skill: shooter), and the adapter that lets a
# loaded progs (plugins/progs) drive that loop (skill: progs), and the host of a C++ mod on the
# same seam (sim/mod_host.hpp, skill: mod). Its content registry
# (content/) is data and builds nothing. Its sound (sound/) is a second library: what the game
# plays on the engine's synth, over the brush world's leaves. Its server (server/) is a library and
# the headless `shooters_edge_server` executable. Its HUD (hud/) is a library
# of its own, and so are its held weapons (view/). Its bots (bots/) are a library over the server. mods/ holds the sample C++ mod, built as a package (cmake/Mods.cmake) the tests and
# the `mod` lab load.
file(GLOB SHOOTERS_EDGE_SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sim/*.cpp)
add_library(shooters_edge_sim STATIC ${SHOOTERS_EDGE_SIM_SOURCES})
target_include_directories(shooters_edge_sim PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_sim PUBLIC ge_core ge_console ge_mod ge_shooter ge_progs)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_sim)

file(GLOB SHOOTERS_EDGE_SOUND_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sound/*.cpp)
add_library(shooters_edge_sound STATIC ${SHOOTERS_EDGE_SOUND_SOURCES})
target_include_directories(shooters_edge_sound PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_sound PUBLIC ge_audio ge_brush)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_sound)

# Its server (server/): the host that runs the classic loop on the run clock with its slots, the
# bots' seam and the input log, and the mod server over it (the builtins, hooks and extension
# fields a multiplayer mod calls, skill: progs), as a library the labs and the tests link (skill:
# shooter).
file(GLOB SHOOTERS_EDGE_SERVER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/server/*.cpp)
add_library(shooters_edge_server STATIC ${SHOOTERS_EDGE_SERVER_SOURCES})
target_link_libraries(shooters_edge_server PUBLIC shooters_edge_sim)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_server)

# Its bots (bots/): the pilot (plugins/shooter/pilot.hpp) played on the game's maps, on the nav
# course through the move and on the arena through the server's bot slots, for the tests and the
# `se-pilot` lab (skills: shooter, bots).
file(GLOB SHOOTERS_EDGE_BOTS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bots/*.cpp)
add_library(shooters_edge_bots STATIC ${SHOOTERS_EDGE_BOTS_SOURCES})
target_link_libraries(shooters_edge_bots PUBLIC shooters_edge_server)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_bots)

# The headless dedicated server (server/bin/), named `shooters_edge_server` on disk, at the build's
# root (build/<preset>/shooters_edge_server): `--help` lists its flags.
add_executable(shooters_edge_dedicated server/bin/main.cpp)
target_link_libraries(shooters_edge_dedicated PRIVATE shooters_edge_server)
set_target_properties(shooters_edge_dedicated PROPERTIES OUTPUT_NAME shooters_edge_server
                                                         RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

# Its HUD (hud/): the player's status snapshot, the HUD's pictures (the free set painted in code),
# and the three looks over the engine's HUD layout with their hud_ cvars. No GPU: the CPU raster
# draws it in the `se-hud` lab and the tests (skill: ui). Headers are spelled
# "shooters_edge/hud/<file>.hpp".
file(GLOB SHOOTERS_EDGE_HUD_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hud/*.cpp)
add_library(shooters_edge_hud STATIC ${SHOOTERS_EDGE_HUD_SOURCES})
target_include_directories(shooters_edge_hud PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_hud PUBLIC ge_hud ge_console ge_assets)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_hud)

# Its held weapons (view/): the free set's mounts, kicks and flashes, the hand that switches and
# fires them, and the player's view-model cvars, over engine/viewmodel. No GPU: the `se-viewmodel`
# lab draws them on the CPU's stage (skills: render, shooter). Headers are spelled
# "shooters_edge/view/<file>.hpp".
file(GLOB SHOOTERS_EDGE_VIEW_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/view/*.cpp)
add_library(shooters_edge_view STATIC ${SHOOTERS_EDGE_VIEW_SOURCES})
target_include_directories(shooters_edge_view PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_view PUBLIC ge_viewmodel ge_console ge_assets)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_view)

include(${PROJECT_SOURCE_DIR}/cmake/Mods.cmake)
ge_add_mod(shooters_edge_mod_rocket_bounce NAME rocket-bounce VERSION 1.0.0 GAME shooters_edge
  GAME_API 1 SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mods/rocket_bounce/rocket_bounce.cpp
  INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/games)
