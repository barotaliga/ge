# Shooter's Edge, the engine's second game (docs/shooters-edge.md). Its simulation (sim/) is one
# static library the labs and the tests link: the native game program, the test arena and the
# scripted match over plugins/shooter's classic loop (skill: shooter). Its content registry
# (content/) is data and builds nothing. Its sound (sound/) is a second library: what the game
# plays on the engine's synth, over the brush world's leaves.
file(GLOB SHOOTERS_EDGE_SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sim/*.cpp)
add_library(shooters_edge_sim STATIC ${SHOOTERS_EDGE_SIM_SOURCES})
target_include_directories(shooters_edge_sim PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_sim PUBLIC ge_core ge_shooter)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_sim)

file(GLOB SHOOTERS_EDGE_SOUND_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sound/*.cpp)
add_library(shooters_edge_sound STATIC ${SHOOTERS_EDGE_SOUND_SOURCES})
target_include_directories(shooters_edge_sound PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_sound PUBLIC ge_audio ge_brush)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_sound)
