# Proxy (engine/proxy, docs/tv.md § Proxy): a near and a far hop that repeat and pack a game's
# datagrams over the slow link between them, over engine/net. Published as ge::proxy.
file(GLOB GE_PROXY_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/proxy/*.cpp)
add_library(ge_proxy STATIC ${GE_PROXY_SOURCES})
target_link_libraries(ge_proxy PUBLIC ge_net)

ge_module(ge_proxy EXPORT proxy)
