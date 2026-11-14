# The hub (engine/hub, docs/hub.md): a community's website of every live game, as functions of
# the masters' lists and the nodes' status records: the poller, the index, the JSON API and the
# HTTP that carries it. A game's `<game>_hub` binary is made by `ge_add_hub` (cmake/Hub.cmake).
# Not simulation code: the process paces itself by the wall clock (the poller never reads it).
file(GLOB GE_HUB_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/hub/*.cpp)
add_library(ge_hub STATIC ${GE_HUB_SOURCES})
target_link_libraries(ge_hub PUBLIC ge_core ge_net ge_forward ge_replay ge_mod ge_account ge_vfs)

ge_module(ge_hub EXPORT hub)
include(cmake/Hub.cmake)
