# The light bake (engine/bake, docs/rendering.md § Baked light): a static world's light worked out
# once on the CPU into a lightmap atlas (charts, a BVH, direct light, colour bounce, the light's
# direction a luxel). Deterministic for any number of workers, no platform. Simulation code: its
# lightmaps feed lab digests and tests. Published as ge::bake.
file(GLOB GE_BAKE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/bake/*.cpp)
add_library(ge_bake STATIC ${GE_BAKE_SOURCES})
target_link_libraries(ge_bake PUBLIC ge_core)

ge_module(ge_bake EXPORT bake SIMULATION)
