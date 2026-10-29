# Snow (engine/snow, docs/snow.md): a layer of snow over any ground, its sparse fixed-point tiles,
# the one sample query, press and the imprint record, the snapshot. Deterministic, no platform;
# published as ge::snow.
file(GLOB GE_SNOW_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/snow/*.cpp)
add_library(ge_snow STATIC ${GE_SNOW_SOURCES})
target_link_libraries(ge_snow PUBLIC ge_core)

ge_module(ge_snow EXPORT snow SIMULATION)
