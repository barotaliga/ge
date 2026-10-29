# Shooter's Edge, the engine's second game (docs/shooters-edge.md). Its simulation (sim/) is one
# static library the labs and the tests link: the native game program, the test arena and the
# scripted match over plugins/shooter's classic loop (skill: shooter), and the adapter that lets a
# loaded progs (plugins/progs) drive that loop (skill: progs). Its content registry
# (content/) is data and builds nothing.
file(GLOB SHOOTERS_EDGE_SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sim/*.cpp)
add_library(shooters_edge_sim STATIC ${SHOOTERS_EDGE_SIM_SOURCES})
target_include_directories(shooters_edge_sim PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_sim PUBLIC ge_core ge_shooter ge_progs)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_sim)
