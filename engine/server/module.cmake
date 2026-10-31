# The server core (engine/server, docs/servers.md): one game's server as functions it plugs in
# (ServerGame), the frame, the node, session and hosting round it, and the dedicated process's
# main. A game's `<game>_server` binary is made by `ge_add_server` (cmake/Server.cmake). Not
# simulation code: the process paces frames by the wall clock (the frame itself never reads it).
file(GLOB GE_SERVER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/server/*.cpp)
add_library(ge_server STATIC ${GE_SERVER_SOURCES})
target_link_libraries(ge_server PUBLIC ge_core ge_console ge_net)

ge_module(ge_server EXPORT server)
include(cmake/Server.cmake)
