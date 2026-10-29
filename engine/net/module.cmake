# The network (engine/net, docs/networking.md): addresses, the wire, the OS's UDP and TCP sockets
# (os_socket.hpp the only file that includes the OS's headers), the simulated network, captures, connections and nodes. No platform
# layer and no clock: the caller passes the time, so a session over the simulated network is
# deterministic.
file(GLOB GE_NET_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/net/*.cpp)
add_library(ge_net STATIC ${GE_NET_SOURCES})
target_link_libraries(ge_net PUBLIC ge_core ge_console)
if(WIN32)
  target_link_libraries(ge_net PRIVATE ws2_32)
endif()

ge_module(ge_net EXPORT net SIMULATION)
