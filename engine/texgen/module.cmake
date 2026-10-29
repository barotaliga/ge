# Procedural textures (engine/texgen, docs/textures.md): a recipe an agent writes as text,
# seeded and layered (noise, patterns, dirt, dust, wear, glow) and baked to the maps a renderer
# samples, cached on disk as PNG. Deterministic, no platform; links ge_lab for the PNG writer.
# Simulation code: its bakes are goldens and lab digests. Published as ge::texgen.
file(GLOB GE_TEXGEN_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/texgen/*.cpp)
add_library(ge_texgen STATIC ${GE_TEXGEN_SOURCES})
target_link_libraries(ge_texgen PUBLIC ge_core ge_lab)

ge_module(ge_texgen EXPORT texgen SIMULATION)
