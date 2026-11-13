# WebTransport (engine/webtransport, docs/hub.md § Playing in a browser): the endpoint a game's page
# in a browser opens a session to, its datagrams going on as UDP through engine/forward. QUIC and
# TLS come from ngtcp2 and picotls (cmake/Quic.cmake), whose objects are built into
# ge_webtransport. Native only: a page is the other end, never this.
if(NOT GE_WEB)
  include(cmake/Quic.cmake)
  file(GLOB GE_WEBTRANSPORT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/webtransport/*.cpp)
  add_library(ge_webtransport STATIC ${GE_WEBTRANSPORT_SOURCES} $<TARGET_OBJECTS:ge_quic>)
  target_link_libraries(ge_webtransport PUBLIC ge_core ge_net)
  # Their headers as the system's: their warnings are not ours (the tree builds -Wpedantic).
  target_include_directories(ge_webtransport SYSTEM PRIVATE ${GE_QUIC_INCLUDES})
  target_compile_definitions(ge_webtransport PRIVATE ${GE_QUIC_DEFINES})
  if(WIN32)
    target_link_libraries(ge_webtransport PRIVATE bcrypt)
  endif()
endif()

ge_module(ge_webtransport EXPORT webtransport)
