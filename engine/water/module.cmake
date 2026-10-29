# The water (engine/water): bodies of water, the sea a wind grows on them and the one `sample`
# query, and the sound of it on the synth (docs/water.md). Deterministic, no platform; published
# as ge::water.
file(GLOB GE_WATER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/water/*.cpp)
add_library(ge_water STATIC ${GE_WATER_SOURCES})
target_link_libraries(ge_water PUBLIC ge_core ge_wind ge_audio)

ge_module(ge_water EXPORT water SIMULATION)
