# Shooter's Edge, the engine's second game (docs/shooters-edge.md). Its simulation (sim/) is one
# static library the labs and the tests link: the native game program, the test arena and the
# scripted match over plugins/shooter's classic loop (skill: shooter), and the adapter that lets a
# loaded progs (plugins/progs) drive that loop (skill: progs), and the host of a C++ mod on the
# same seam (sim/mod_host.hpp, skill: mod). Its content registry
# (content/) is data and builds nothing. Its sound (sound/) is a second library: what the game
# plays on the engine's synth, over the brush world's leaves. Its server (server/) is a library and
# the headless `shooters_edge_server` executable. Its HUD (hud/) is a library
# of its own, and so are its held weapons (view/). Its bots (bots/) are a library over the server; its app half (app/) is the picture's game side, with the GPU. mods/ holds the sample C++ mod, built as a package (cmake/Mods.cmake) the tests and
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

# The classic protocol (net/classic_*): the codec, the net channel, the classic server over the
# QC server on engine/net's sockets, the scripted client and the measured trial (skill: net). Only
# the classic_ files: net/host_* belong to the host's own library.
file(GLOB SHOOTERS_EDGE_CLASSIC_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/net/classic_*.cpp)
add_library(shooters_edge_classic STATIC ${SHOOTERS_EDGE_CLASSIC_SOURCES})
target_link_libraries(shooters_edge_classic PUBLIC shooters_edge_server ge_net)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_classic)

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

# Its main menu (menu/): the classic pages wired to the console's cvars and commands, the rows the
# game cannot do yet dimmed, drawn in the engine's menu view or in the classic pack's own pictures,
# with classic-enhance's menu under OPTIONS. No GPU: the `se-menu` lab and the tests draw it on the
# CPU (skill: ui). Headers are spelled "shooters_edge/menu/<file>.hpp".
file(GLOB SHOOTERS_EDGE_MENU_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/menu/*.cpp)
add_library(shooters_edge_menu STATIC ${SHOOTERS_EDGE_MENU_SOURCES})
target_include_directories(shooters_edge_menu PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_menu PUBLIC ge_ui ge_console ge_vfs ge_classicenhance)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_menu)

# Its app half (app/): the picture's game side over the engine's passes and the brush world's
# renderer: the effects (trails, bursts, dynamic lights, the screen's blend) and the looks. It
# needs the GPU; the game's GPU labs link it. Headers are spelled "shooters_edge/app/<file>.hpp".
if(TARGET ge_brush_app)
  file(GLOB SHOOTERS_EDGE_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp)
  add_library(shooters_edge_app STATIC ${SHOOTERS_EDGE_APP_SOURCES})
  target_include_directories(shooters_edge_app PUBLIC ${PROJECT_SOURCE_DIR}/games)
  target_link_libraries(shooters_edge_app PUBLIC ge_render ge_brush_app ge_classicenhance ge_console
                                                ge_morph)
  set_property(GLOBAL APPEND PROPERTY GE_GAME_APPS shooters_edge_app)
endif()

include(${PROJECT_SOURCE_DIR}/cmake/Mods.cmake)
ge_add_mod(shooters_edge_mod_rocket_bounce NAME rocket-bounce VERSION 1.0.0 GAME shooters_edge
  GAME_API 1 SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mods/rocket_bounce/rocket_bounce.cpp
  INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/games)

# Its client (client/): the listen server and its local player (listen.hpp), the search path over
# a game folder's packs (files.hpp), the player's keys and mouse (controls.hpp) and what he sees of
# the server (view.hpp): a library with no GPU, which the tests drive headless; the baseline mod's
# QC sources are built into it, so the game compiles its mod at start and reads no source file.
# Then the `shooters_edge` window (client/main.cpp, the world drawn by client/world_draw.cpp) when
# the GPU and the platform are built. Headers are spelled "shooters_edge/client/<file>.hpp".
file(GLOB SHOOTERS_EDGE_CLIENT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/client/*.cpp)
list(FILTER SHOOTERS_EDGE_CLIENT_SOURCES EXCLUDE REGEX
     "/client/(main|world_draw|world_extras|world_fx|world_models)\\.cpp$")
add_library(shooters_edge_client STATIC ${SHOOTERS_EDGE_CLIENT_SOURCES})
target_link_libraries(shooters_edge_client PUBLIC shooters_edge_server shooters_edge_hud ge_vfs
                                                  ge_brush ge_camera ge_input)
include(${PROJECT_SOURCE_DIR}/cmake/EmbedBinary.cmake)
foreach(qc defs util combat weapons items player world)
  ge_embed_binary(shooters_edge_client se_qc_${qc} ${CMAKE_CURRENT_SOURCE_DIR}/qc/${qc}.qc)
endforeach()
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_client)
# The world's frame (client/world_draw.cpp), the modern look's water, liquids, flames and weather
# behind the classic-enhance switches (client/world_extras.cpp), the effects fed to it
# (client/world_fx.cpp) and the models standing in it (client/world_models.cpp, the packs' own)
# are a library of their own with the GPU, which the window and the game's GPU lab (`se-client`,
# labs/gpu_se_client_lab.cpp) link.
if(GE_GPU AND TARGET ge_brush_app)
  add_library(shooters_edge_client_gpu STATIC client/world_draw.cpp client/world_extras.cpp
                                              client/world_fx.cpp client/world_models.cpp)
  target_link_libraries(shooters_edge_client_gpu PUBLIC shooters_edge_client shooters_edge_app
                                                        ge_brush_app ge_classicenhance ge_render
                                                        ge_ui ge_morph)
  set_property(GLOBAL APPEND PROPERTY GE_GAME_APPS shooters_edge_client_gpu)
endif()
if(GE_PLATFORM AND GE_GPU AND TARGET ge_brush_app)
  add_executable(shooters_edge client/main.cpp)
  target_link_libraries(shooters_edge PRIVATE shooters_edge_client_gpu shooters_edge_menu
                                              ge_platform ge_hud ge_console)
endif()
