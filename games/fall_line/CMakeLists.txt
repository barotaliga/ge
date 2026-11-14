# Fall Line, the engine's demo game. Its simulation (sim/) is one static library the labs and
# the tests link: create_game, the step's order, the course, the bot and simulate (skill: sim).
# A game is the demo, never part of the published engine.
file(GLOB_RECURSE FALL_LINE_SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sim/*.cpp)
add_library(fall_line_sim STATIC ${FALL_LINE_SIM_SOURCES})
target_include_directories(fall_line_sim PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(fall_line_sim PUBLIC fall_line_cfg ge_core ge_prof ge_play ge_replay ge_racing ge_mountain ge_skier ge_snowmobile ge_helicopter ge_paramotor)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS fall_line_sim)

# Its cvars (cfg/): every setting a cfg may change, bound to one Settings struct the app reads and
# the play executable registers (skill: console). No GPU.
file(GLOB FALL_LINE_CFG_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cfg/*.cpp)
add_library(fall_line_cfg STATIC ${FALL_LINE_CFG_SOURCES})
target_include_directories(fall_line_cfg PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(fall_line_cfg PUBLIC ge_console ge_camera ge_play)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS fall_line_cfg)

# Its `play` executable (play/): the game on the play contract, which `ge play`, `ge sim`,
# `ge replay`, `ge state` and `ge diverge` build and run (skill: agent-interface).
# It lands at the build's root (build/<preset>/play), where ge looks for a game's play target.
# A host tool: not built for a mobile target (GE_HOST_TOOLS).
if(GE_HOST_TOOLS)
  add_executable(play play/main.cpp)
  target_link_libraries(play PRIVATE fall_line_sim fall_line_cfg)
  set_target_properties(play PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

# Its sound (sound/): the bank, the event route, the listener and the faders, over the skier's
# beds. No GPU and no device, so the tests and the `audition` lab link it headless (skill: audio).
file(GLOB FALL_LINE_SOUND_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sound/*.cpp)
add_library(fall_line_sound STATIC ${FALL_LINE_SOUND_SOURCES})
target_link_libraries(fall_line_sound PUBLIC fall_line_sim ge_skier_app ge_audio)
# The play executable writes a run with its sound (`ge record --format=mkv`).
if(TARGET play)
  target_link_libraries(play PRIVATE fall_line_sound)
endif()
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS fall_line_sound)

# Its camera (camera/): the ladder of engine rigs a run is seen from, the skier as the camera's
# subject, the mountain's answers to it. No GPU, so the tests and the labs link it headless
# (skill: camera).
file(GLOB FALL_LINE_CAMERA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/camera/*.cpp)
add_library(fall_line_camera STATIC ${FALL_LINE_CAMERA_SOURCES})
target_link_libraries(fall_line_camera PUBLIC fall_line_sim ge_skier_app ge_camera)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS fall_line_camera)

# Its controls (controls/): the actions a person plays with, the default bindings file, what is
# felt, the rebinding walk and the run played by hand on the play contract. No GPU and no device,
# so the tests and the labs drive it with simulated devices (skill: input).
file(GLOB FALL_LINE_CONTROLS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/controls/*.cpp)
add_library(fall_line_controls STATIC ${FALL_LINE_CONTROLS_SOURCES})
target_link_libraries(fall_line_controls PUBLIC fall_line_sim ge_input ge_play)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS fall_line_controls)

# Its HUD (hud/): the free ride's figures read off the state, its words, its news and its view
# over the engine's HUD kit. No GPU: the CPU raster draws it in the `hud` lab and the tests
# (skill: ui).
file(GLOB FALL_LINE_HUD_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hud/*.cpp)
add_library(fall_line_hud STATIC ${FALL_LINE_HUD_SOURCES})
target_link_libraries(fall_line_hud PUBLIC fall_line_sim ge_hud)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS fall_line_hud)

# Its app half: `fall_line_app`, the game's frame (its pass table over engine/render, the look it hands the
# passes, its named views, its forest and its skinned skiers), which the `fall_line` window and the game's GPU labs both link; it
# needs the GPU. Headers are spelled "fall_line/app/<file>.hpp".
if(GE_GPU)
  file(GLOB FALL_LINE_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp)
  list(FILTER FALL_LINE_APP_SOURCES EXCLUDE REGEX "/app/main\\.cpp$")
  add_library(fall_line_app STATIC ${FALL_LINE_APP_SOURCES})
  target_include_directories(fall_line_app PUBLIC ${PROJECT_SOURCE_DIR}/games)
  target_link_libraries(fall_line_app PUBLIC ge_render ge_prof ge_mountain_app ge_skier_app fall_line_sim fall_line_camera fall_line_cfg)
  # The skier model the racers are drawn as (scene.hpp), compiled in: the game reads no file.
  ge_embed_binary(fall_line_app fall_line_skier_glb
    ${PROJECT_SOURCE_DIR}/plugins/skier/assets/skier/skier.glb)
  set_property(GLOBAL APPEND PROPERTY GE_GAME_APPS fall_line_app)
  # The play executable draws the GPU's pictures too (`ge shot --gpu`, `ge record --gpu`).
  if(TARGET play)
    target_link_libraries(play PRIVATE fall_line_app)
    target_compile_definitions(play PRIVATE FALL_LINE_PLAY_GPU=1)
  endif()
  # Its web viewer (web/viewer.cpp): a run's input log watched again on a hub's page; made only by
  # a `web` build of this game (`cmake --preset web -DGE_WEB_GAME=fall_line`).
  ge_add_web_viewer(fall_line SOURCES web/viewer.cpp LIBS fall_line_app ge_replay)
endif()
if(GE_PLATFORM AND GE_GPU)
  add_executable(fall_line app/main.cpp)
  ge_app_bundle(fall_line NAME "Fall Line" ID fall-line)
  target_link_libraries(fall_line PRIVATE ge_platform fall_line_app fall_line_sound fall_line_cfg
                                          fall_line_controls)
endif()

# The hub (hub/), named `fall_line_hub` on disk at the build's root: the engine's hub (engine/hub,
# docs/hub.md) with Fall Line's words, keeping the game's runs (input logs) and showing them with
# the web viewer. No live games until Fall Line has a dedicated server (M17k). A host tool.
if(GE_HOST_TOOLS AND TARGET ge_hub)
  add_library(fall_line_hub_runs STATIC hub/runs.cpp)
  target_link_libraries(fall_line_hub_runs PUBLIC fall_line_sim ge_hub)
  set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS fall_line_hub_runs)  # its test links it
  ge_add_hub(fall_line_hub_bin OUTPUT_NAME fall_line_hub SOURCES hub/main.cpp
             LINK fall_line_hub_runs)
endif()
