# TV (engine/tv, docs/tv.md): a match's replay file stream from a source, held by a delay and fanned
# out to viewers and to other TV nodes, over engine/net. Published as ge::tv.
file(GLOB GE_TV_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/tv/*.cpp)
add_library(ge_tv STATIC ${GE_TV_SOURCES})
target_link_libraries(ge_tv PUBLIC ge_replay ge_net)

ge_module(ge_tv EXPORT tv)
