# Particle effects as data (engine/particles): the effect text a game or a mod ships, read into a
# book, and a burst or a trail thrown from it on a seed (effect.hpp). Presentation, no platform and
# no GPU: the specks it throws are copied into the render pool by render/spark.hpp. Published as ge::particles.
file(GLOB GE_PARTICLES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/particles/*.cpp)
add_library(ge_particles STATIC ${GE_PARTICLES_SOURCES})
target_link_libraries(ge_particles PUBLIC ge_core)

ge_module(ge_particles EXPORT particles)
