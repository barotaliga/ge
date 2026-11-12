# Forward (engine/forward, docs/tv.md § Forward): an opaque hop for any game's datagrams, chained by
# the route set up before the game's own handshake, over engine/net. Published as ge::forward.
file(GLOB GE_FORWARD_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/forward/*.cpp)
add_library(ge_forward STATIC ${GE_FORWARD_SOURCES})
target_link_libraries(ge_forward PUBLIC ge_net)

ge_module(ge_forward EXPORT forward)
