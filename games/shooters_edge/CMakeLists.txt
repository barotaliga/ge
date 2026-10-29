# Shooter's Edge, the engine's second game (docs/shooters-edge.md). Its simulation (sim/) is one
# static library the labs and the tests link: the native game program, the test arena and the
# scripted match over plugins/shooter's classic loop (skill: shooter). Its content registry
# (content/) is data and builds nothing. Its server (server/) is a library and the headless
# `shooters_edge_server` executable.
file(GLOB SHOOTERS_EDGE_SIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sim/*.cpp)
add_library(shooters_edge_sim STATIC ${SHOOTERS_EDGE_SIM_SOURCES})
target_include_directories(shooters_edge_sim PUBLIC ${PROJECT_SOURCE_DIR}/games)
target_link_libraries(shooters_edge_sim PUBLIC ge_core ge_shooter)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_sim)

# Its server (server/): the host that runs the classic loop on the run clock with its slots, the
# bots' seam and the input log, as a library the labs and the tests link (skill: shooter).
file(GLOB SHOOTERS_EDGE_SERVER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/server/*.cpp)
add_library(shooters_edge_server STATIC ${SHOOTERS_EDGE_SERVER_SOURCES})
target_link_libraries(shooters_edge_server PUBLIC shooters_edge_sim)
set_property(GLOBAL APPEND PROPERTY GE_GAME_TARGETS shooters_edge_server)

# The headless dedicated server (server/bin/), named `shooters_edge_server` on disk, at the build's
# root (build/<preset>/shooters_edge_server): `--help` lists its flags.
add_executable(shooters_edge_dedicated server/bin/main.cpp)
target_link_libraries(shooters_edge_dedicated PRIVATE shooters_edge_server)
set_target_properties(shooters_edge_dedicated PROPERTIES OUTPUT_NAME shooters_edge_server
                                                         RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
