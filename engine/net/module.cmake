# The network (engine/net, docs/networking.md): addresses, the wire, the OS's UDP and TCP sockets
# (os_socket.hpp the only file that includes the OS's headers), the simulated network, captures, connections and nodes. No platform
# layer and no clock: the caller passes the time, so a session over the simulated network is
# deterministic. Voice chat (net/voice.hpp) plays its talkers on the synth (ge_audio); its codec is
# libopus (cmake/Codecs.cmake, GE_ENCODERS), linked privately and only in this tree's own build:
# without it the voice runs on a stand-in.
file(GLOB GE_NET_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/net/*.cpp)
add_library(ge_net STATIC ${GE_NET_SOURCES})
target_link_libraries(ge_net PUBLIC ge_core ge_console ge_audio)
if(WIN32)
  target_link_libraries(ge_net PRIVATE ws2_32)
endif()
include(cmake/Codecs.cmake)
if(GE_ENCODERS)
  target_link_libraries(ge_net PRIVATE $<BUILD_INTERFACE:ge::opus>)
  target_compile_definitions(ge_net PRIVATE GE_ENCODERS=1)
endif()

ge_module(ge_net EXPORT net SIMULATION)
