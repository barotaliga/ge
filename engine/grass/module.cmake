# The grass (engine/grass, docs/grass.md): kinds of grass, the covers they grow on (a heightfield's
# region or a mesh's floors), every blade scattered off its triangle's seed, and its sway in the
# engine's wind. Deterministic, no platform. Simulation code: its blades feed lab digests and
# goldens. Published as ge::grass.
file(GLOB GE_GRASS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/grass/*.cpp)
add_library(ge_grass STATIC ${GE_GRASS_SOURCES})
target_link_libraries(ge_grass PUBLIC ge_core ge_wind)

ge_module(ge_grass EXPORT grass SIMULATION)
