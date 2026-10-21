# Fall Line, the engine's demo game. Its simulation (sim/) is one static library the labs and
# the tests link: create_game, the step's order, the course, the bot and simulate (skill: sim).
# A game is the demo, never part of the published engine.
file(GLOB_RECURSE FALL_LINE_SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sim/*.cpp)
add_library(fall_line_sim STATIC ${FALL_LINE_SIM_SOURCES})
target_include_directories(fall_line_sim PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(fall_line_sim PUBLIC ge_core ge_racing ge_mountain ge_skier)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS fall_line_sim)

# Its app half: `fall_line_app`, the game's frame (its pass table over engine/render, the look it hands the
# passes, its named views), which the `fall_line` window and the game's GPU labs both link; it
# needs the GPU. Headers are spelled "fall_line/app/<file>.hpp".
if(GE_GPU)
  file(GLOB FALL_LINE_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp)
  list(FILTER FALL_LINE_APP_SOURCES EXCLUDE REGEX "/app/main\\.cpp$")
  add_library(fall_line_app STATIC ${FALL_LINE_APP_SOURCES})
  target_include_directories(fall_line_app PUBLIC ${PROJECT_SOURCE_DIR}/games)
  target_link_libraries(fall_line_app PUBLIC ge_render ge_mountain)
  set_property(GLOBAL APPEND PROPERTY GE_GAME_APPS fall_line_app)
endif()
if(GE_PLATFORM AND GE_GPU)
  add_executable(fall_line app/main.cpp)
  target_link_libraries(fall_line PRIVATE ge_platform fall_line_app)
endif()
