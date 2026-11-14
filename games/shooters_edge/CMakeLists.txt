# Shooter's Edge, the engine's second game (docs/shooters-edge.md). Its simulation (sim/) is one
# static library the labs and the tests link: the native game program, the test arena and the
# scripted match over plugins/shooter's classic loop (skill: shooter), and the adapter that lets a
# loaded progs (plugins/progs) drive that loop (skill: progs), and the host of a C++ mod on the
# same seam (sim/mod_host.hpp, skill: mod). Its content registry
# (content/) is data and builds nothing. Its sound (sound/) is a second library: what the game
# plays on the engine's synth, over the brush world's leaves. Its server (server/) is a library and
# the headless `sesv` executable. Its HUD (hud/) is a library
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
target_link_libraries(shooters_edge_server PUBLIC shooters_edge_sim shooters_edge_rules ge_save ge_server)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_server)

# Its own rules in C++ (rules/, M52h): native programs on the mod server (progs/native.hpp), the
# arena's deathmatch first, as a library the server links (skills: progs, shooter).
file(GLOB SHOOTERS_EDGE_RULES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rules/*.cpp)
add_library(shooters_edge_rules STATIC ${SHOOTERS_EDGE_RULES_SOURCES})
target_include_directories(shooters_edge_rules PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_rules PUBLIC ge_progs)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_rules)

# Its bots (bots/): the pilot (plugins/shooter/pilot.hpp) played on the game's maps, on the nav
# course through the move and on the arena through the server's bot slots, for the tests and the
# `se-pilot` lab (skills: shooter, bots).
file(GLOB SHOOTERS_EDGE_BOTS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bots/*.cpp)
add_library(shooters_edge_bots STATIC ${SHOOTERS_EDGE_BOTS_SOURCES})
target_link_libraries(shooters_edge_bots PUBLIC shooters_edge_server)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_bots)

# The classic protocol (net/classic_*): the codec, the net channel, the classic server over the
# QC server on engine/net's sockets, the scripted client and the measured trial (skill: net); and
# the sequel's live protocol beside it (net/sequel_*: its client and server). Only the classic_ and
# sequel_ files: net/host_* belong to the host's own library.
file(GLOB SHOOTERS_EDGE_CLASSIC_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/net/classic_*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/net/sequel_*.cpp)
add_library(shooters_edge_classic STATIC ${SHOOTERS_EDGE_CLASSIC_SOURCES})
target_link_libraries(shooters_edge_classic PUBLIC shooters_edge_server shooters_edge_client ge_net ge_play
                      ge_forward ge_tv)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_classic)

# The headless dedicated server (server/bin/), named `sesv` on disk, at the build's root
# (build/<preset>/sesv): the engine's dedicated process (engine/server) over the game's playable
# ServerGame (net/classic_game.hpp: the mod's deathmatch on the classic protocol), or with
# `--sequel` the same core over the sequel's rules on its own protocol (net/sequel_game.hpp), so an
# existing client of the sequel joins it. `--help` lists its flags.
ge_add_server(shooters_edge_dedicated OUTPUT_NAME sesv SOURCES server/bin/main.cpp
              LINK shooters_edge_classic)

# The hub (hub/), named `shooters_edge_hub` on disk at the build's root: the engine's hub
# (engine/hub, docs/hub.md) with the game's modes and words. `--help` lists its flags.
# The classic protocol's library gives it the classic status query (net/classic_browse,
# net/classic_info) and the match a Play page's map is started on to name its files
# (client/play_content.hpp); the client's demo reader names a demo's (client/demo_content.hpp).
ge_add_hub(shooters_edge_hub_bin OUTPUT_NAME shooters_edge_hub SOURCES hub/main.cpp
           LINK shooters_edge_classic)
target_include_directories(shooters_edge_hub_bin PRIVATE ${PROJECT_SOURCE_DIR}/games)

# The bot team, headless (net/bots_main.cpp), named `shooters_edge_bots` on disk at the build's
# root: a team of bots joins a classic server, any server (net/classic_bot_team.hpp). `--help`
# lists its flags.
add_executable(shooters_edge_bots_cli net/bots_main.cpp)
target_link_libraries(shooters_edge_bots_cli PRIVATE shooters_edge_classic ge_flags)
set_target_properties(shooters_edge_bots_cli PROPERTIES OUTPUT_NAME shooters_edge_bots
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# The hops, headless (net/hops_main.cpp, one source built three times), at the build's root: our
# forward (`sefwd`), proxies (`seproxy`) or TV node (`setv`) as a process of its own, for any
# classic client. `SE_HOP` picks which; `--help` lists each one's flags.
foreach(hop sefwd:1 seproxy:2 setv:3)
  string(REPLACE ":" ";" hop "${hop}")
  list(GET hop 0 hop_name)
  list(GET hop 1 hop_id)
  add_executable(shooters_edge_${hop_name} net/hops_main.cpp)
  target_link_libraries(shooters_edge_${hop_name} PRIVATE shooters_edge_classic ge_proxy ge_flags)
  target_compile_definitions(shooters_edge_${hop_name} PRIVATE SE_HOP=${hop_id})
  set_target_properties(shooters_edge_${hop_name} PROPERTIES OUTPUT_NAME ${hop_name}
                        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach()

# Its HUD (hud/): the player's status snapshot, the HUD's pictures (the free set painted in code),
# and the three looks over the engine's HUD layout with their hud_ cvars. No GPU: the CPU raster
# draws it in the `se-hud` lab and the tests (skill: ui). Headers are spelled
# "shooters_edge/hud/<file>.hpp".
file(GLOB SHOOTERS_EDGE_HUD_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hud/*.cpp)
add_library(shooters_edge_hud STATIC ${SHOOTERS_EDGE_HUD_SOURCES})
target_include_directories(shooters_edge_hud PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_hud PUBLIC ge_hud ge_console ge_assets ge_vfs)
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
target_link_libraries(shooters_edge_menu PUBLIC ge_ui ge_console ge_vfs ge_net ge_classicenhance
                                            ge_assets ge_play shooters_edge_hud)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_menu)
# SELECT GAME's pictures of the games' names, by content profile (menu/games_page.cpp).
include(${PROJECT_SOURCE_DIR}/cmake/EmbedBinary.cmake)
foreach(name classic sequel)
  ge_embed_binary(shooters_edge_menu se_game_name_${name}
                  ${CMAKE_CURRENT_SOURCE_DIR}/menu/art/${name}.png)
endforeach()

# Its app half (app/): the picture's game side over the engine's passes and the brush world's
# renderer: the effects (trails, bursts, dynamic lights, the screen's blend) and the looks. It
# needs the GPU; the game's GPU labs link it. Headers are spelled "shooters_edge/app/<file>.hpp".
if(TARGET ge_brush_app)
  file(GLOB SHOOTERS_EDGE_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp)
  add_library(shooters_edge_app STATIC ${SHOOTERS_EDGE_APP_SOURCES})
  target_include_directories(shooters_edge_app PUBLIC ${PROJECT_SOURCE_DIR}/games)
  target_link_libraries(shooters_edge_app PUBLIC ge_render ge_brush_app ge_classicenhance ge_console
                                                ge_morph ge_assets)
  set_property(GLOBAL APPEND PROPERTY GE_GAME_APPS shooters_edge_app)
endif()

include(${PROJECT_SOURCE_DIR}/cmake/Mods.cmake)
ge_add_mod(shooters_edge_mod_rocket_bounce NAME rocket-bounce VERSION 1.0.0 GAME shooters_edge
  GAME_API 1 SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mods/rocket_bounce/rocket_bounce.cpp
  INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/games)

# Its client (client/): the listen server and its local player (listen.hpp), the search path over
# a game folder's packs (files.hpp), the player's keys and mouse (controls.hpp) and what he sees of
# the server (view.hpp): a library with no GPU, which the tests drive headless.
# Then the `shooters_edge` window (client/main.cpp, the world drawn by client/world_draw.cpp) when
# the GPU and the platform are built. Headers are spelled "shooters_edge/client/<file>.hpp".
file(GLOB SHOOTERS_EDGE_CLIENT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/client/*.cpp)
list(FILTER SHOOTERS_EDGE_CLIENT_SOURCES EXCLUDE REGEX
     "/client/(main|window_cvars|bot_view|world_draw|world_extras|world_fx|world_fx_book|world_models|burning|demo_video|title|map_load)\.cpp$")
add_library(shooters_edge_client STATIC ${SHOOTERS_EDGE_CLIENT_SOURCES})
target_link_libraries(shooters_edge_client PUBLIC shooters_edge_server shooters_edge_bots shooters_edge_hud
                                                  shooters_edge_view shooters_edge_sound ge_vfs
                                                  ge_brush ge_camera ge_input ge_audio ge_net
                                                  ge_ui ge_replay ge_morph shooters_edge_menu
                                                  ge_classicenhance)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_client)
# The world's frame (client/world_draw.cpp), the modern look's water, liquids, flames and weather
# behind the classic-enhance switches (client/world_extras.cpp), the effects fed to it
# (client/world_fx.cpp), the models standing in it (client/world_models.cpp, the packs' own) and
# a demo turned into a video (client/demo_video.cpp, through engine/capture), the title's night
# (client/title.cpp) and a map loaded behind the window (client/map_load.cpp) are a library of their
# own with the GPU, which the window and the game's GPU lab (`se-client`,
# labs/gpu_se_client_lab.cpp) link.
if(GE_GPU AND TARGET ge_brush_app)
  add_library(shooters_edge_client_gpu STATIC client/world_draw.cpp client/world_extras.cpp
                                              client/world_fx.cpp client/world_fx_book.cpp
                                              client/world_models.cpp client/burning.cpp
                                              client/demo_video.cpp client/title.cpp
                                              client/map_load.cpp)
  target_link_libraries(shooters_edge_client_gpu PUBLIC shooters_edge_client shooters_edge_app
                                                        ge_brush_app ge_classicenhance ge_render
                                                        ge_ui ge_morph ge_play ge_detail)
  set_property(GLOBAL APPEND PROPERTY GE_GAME_APPS shooters_edge_client_gpu)
endif()
# The web viewer (web/viewer.cpp, docs/hub.md § Watching in a browser): the world drawn on the hub
# page's canvas, a demo played from the hub's archive; `shooters_edge_viewer.js` and `.wasm` in
# the web build's `shooters_edge_viewer/`, the folder a hub's --viewer=<dir> serves. Beside it the
# player (`shooters_edge_play`, docs/hub.md § Playing in a browser): the same viewer that also
# plays a match on the page against bots (web/play.cpp, its server the classic game's), kept apart
# so a page that only watches downloads no server.
if(TARGET shooters_edge_client_gpu)
  ge_add_web_viewer(shooters_edge SOURCES web/viewer.cpp LIBS shooters_edge_client_gpu ge_tv)
  ge_add_web_viewer(shooters_edge NAME shooters_edge_play SOURCES web/viewer.cpp web/play.cpp
                    LIBS shooters_edge_client_gpu shooters_edge_classic ge_tv
                    DEFINES SE_WEB_PLAY=1)
endif()
if(GE_PLATFORM AND GE_GPU AND TARGET ge_brush_app)
  # The window, `se` on disk at the build's root (build/<preset>/se) beside `sesv` and the hops;
  # the target keeps the game's name, so `make run:shooters_edge` builds it.
  add_executable(shooters_edge client/main.cpp client/window_cvars.cpp client/bot_view.cpp)
  set_target_properties(shooters_edge PROPERTIES OUTPUT_NAME se
                        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  target_link_libraries(shooters_edge PRIVATE shooters_edge_client_gpu shooters_edge_classic
                                              shooters_edge_menu
                                              ge_platform ge_hud ge_console)
  # The held weapons' models and the game's own content (its sounds for the built-in arena)
  # where the tree is, for a game run from somewhere else (-models wins).
  target_compile_definitions(shooters_edge
                             PRIVATE SE_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/content/models/view"
                                     SE_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/content")
endif()

# Its player CLI (cli/): the engine's game CLI (engine/gamecli, docs/gamecli.md) over the game's
# Spec, classic enhance's group and the game's own rows, as a library the tests link and the
# `shooters_edge_cli` executable at the build's root beside the window and the dedicated server
# (skills: agent-interface, shooter).
if(TARGET ge_gamecli)
  add_library(shooters_edge_gamecli STATIC cli/spec.cpp)
  target_link_libraries(shooters_edge_gamecli PUBLIC ge_gamecli ge_classicenhance_cli
                                                     shooters_edge_classic)
  set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_gamecli)
  ge_add_game_cli(shooters_edge_cli OUTPUT_NAME shooters_edge_cli SOURCES cli/main.cpp
                  LINK shooters_edge_gamecli)
endif()
