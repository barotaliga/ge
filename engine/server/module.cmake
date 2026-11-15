# The server core (engine/server, docs/servers.md): one game's server as functions it plugs in
# (ServerGame), the frame, the node, session and hosting round it, and the dedicated process's
# main. A game's `<game>_server` binary is made by `ge_add_server` (cmake/Server.cmake). Not
# simulation code: the process paces frames by the wall clock (the frame itself never reads it).
file(GLOB GE_SERVER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/server/*.cpp)
# Starting a server as its own process (process.cpp, spawn.cpp) is a desktop's: a phone's store
# and a browser start no programs.
if(GE_MOBILE OR GE_WEB)
  list(FILTER GE_SERVER_SOURCES EXCLUDE REGEX "/engine/server/(process|spawn)\\.cpp$")
endif()
add_library(ge_server STATIC ${GE_SERVER_SOURCES})
target_link_libraries(ge_server PUBLIC ge_core ge_console ge_net ge_account ge_replay ge_tv)

ge_module(ge_server EXPORT server)
include(cmake/Server.cmake)

# The sample game's dedicated server (`gesample`, engine/server/bin/sample_main.cpp): what the
# `server-launch` lab starts, joins, finds again and stops, with no real game linked.
if(NOT DEFINED GE_HOST_TOOLS OR GE_HOST_TOOLS)
  ge_add_server(ge_sample_server OUTPUT_NAME gesample
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/engine/server/bin/sample_main.cpp)
endif()
