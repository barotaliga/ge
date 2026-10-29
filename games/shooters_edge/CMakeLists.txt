# Shooter's Edge, the engine's second game (docs/shooters-edge.md). Its content registry
# (content/) is data and builds nothing.
#
# Its simulation (sim/) is one static library the labs and the tests link: the native game
# program, the test arena and the scripted match over plugins/shooter's classic loop (skill:
# shooter).
file(GLOB SHOOTERS_EDGE_SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sim/*.cpp)
add_library(shooters_edge_sim STATIC ${SHOOTERS_EDGE_SIM_SOURCES})
target_include_directories(shooters_edge_sim PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_sim PUBLIC ge_core ge_shooter)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_sim)

# Its HUD (hud/): the player's status snapshot, the HUD's pictures (the free set painted in code),
# and the three looks over the engine's HUD layout with their hud_ cvars. No GPU: the CPU raster
# draws it in the `se-hud` lab and the tests (skill: ui). Headers are spelled
# "shooters_edge/hud/<file>.hpp".
file(GLOB SHOOTERS_EDGE_HUD_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hud/*.cpp)
add_library(shooters_edge_hud STATIC ${SHOOTERS_EDGE_HUD_SOURCES})
target_include_directories(shooters_edge_hud PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_hud PUBLIC ge_hud ge_console ge_assets)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_hud)
