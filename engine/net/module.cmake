# The network (engine/net, docs/networking.md): addresses, the wire, the OS's UDP and TCP sockets
# (os_socket.hpp the only file that includes the OS's headers), the simulated network, captures, connections and nodes. No platform
# layer and no clock: the caller passes the time, so a session over the simulated network is
# deterministic. Voice chat (net/voice.hpp) plays its talkers on the synth (ge_audio); its codec is
# libopus (cmake/Codecs.cmake, GE_ENCODERS), linked privately and only in this tree's own build:
# without it the voice runs on a stand-in. Security (net/crypto.hpp, net/token.hpp) seals with
# Monocypher (cmake/Crypto.cmake), its objects built into ge_net. The web's secure addresses
# (net/tls_stream.hpp) are Mbed TLS's (cmake/Tls.cmake), its objects built into ge_net too.
include(cmake/Crypto.cmake)
include(cmake/Tls.cmake)
file(GLOB GE_NET_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/net/*.cpp)
add_library(ge_net STATIC ${GE_NET_SOURCES} $<TARGET_OBJECTS:ge_monocypher>
  $<TARGET_OBJECTS:ge_mbedtls>)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/engine/net/tls_stream.cpp PROPERTIES
  INCLUDE_DIRECTORIES "${GE_MBEDTLS_INCLUDE}")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/engine/net/crypto.cpp PROPERTIES
  INCLUDE_DIRECTORIES "${GE_MONOCYPHER_INCLUDE}")
target_link_libraries(ge_net PUBLIC ge_core ge_console ge_audio)
if(WIN32)
  # bcrypt: Mbed TLS's random source; crypt32: the system's trusted roots (net/tls_stream.cpp).
  target_link_libraries(ge_net PRIVATE ws2_32 bcrypt crypt32)
endif()
include(cmake/Codecs.cmake)
if(GE_ENCODERS)
  target_link_libraries(ge_net PRIVATE $<BUILD_INTERFACE:ge::opus>)
  target_compile_definitions(ge_net PRIVATE GE_ENCODERS=1)
endif()

ge_module(ge_net EXPORT net SIMULATION)
