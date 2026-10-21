# Fall Line, the engine's demo game. Its simulation (sim/) is one static library the labs and
# the tests link: create_game, the step's order, the course, the bot and simulate (skill: sim).
# A game is the demo, never part of the published engine.
file(GLOB_RECURSE FALL_LINE_SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sim/*.cpp)
add_library(fall_line_sim STATIC ${FALL_LINE_SIM_SOURCES})
target_include_directories(fall_line_sim PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(fall_line_sim PUBLIC ge_core ge_racing ge_mountain ge_skier)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS fall_line_sim)

# Its app shell, `fall_line`: a window drawing the GPU test scene at vsync until the frame core
# draws the mountain (M7). It needs the platform and the GPU.
if(GE_PLATFORM AND GE_GPU)
  add_executable(fall_line app/main.cpp)
  target_link_libraries(fall_line PRIVATE ge_platform ge_gpu)
endif()
