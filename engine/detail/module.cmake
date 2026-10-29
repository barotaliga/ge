# A level's surface detail (engine/detail, docs/detail.md): convex solids and the bevels on their
# exposed edges, decals clipped to surfaces that age in a fixed pool, and props scattered by rules.
# Deterministic, no platform. Simulation code: its props and decals feed lab digests and goldens.
# Published as ge::detail.
file(GLOB GE_DETAIL_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/detail/*.cpp)
add_library(ge_detail STATIC ${GE_DETAIL_SOURCES})
target_link_libraries(ge_detail PUBLIC ge_core)

ge_module(ge_detail EXPORT detail SIMULATION)
