# The wind (engine/wind): a pure function of place and time over a field built once, the
# mountain's air one configuration of it (docs/water.md § Wind), and what falls through it (snow,
# sleet and rain: engine/wind/precip.hpp) and the thunderstorms in it (storm, lightning, thunder:
# docs/storms.md). Deterministic, no platform; published as ge::wind.
add_library(ge_wind STATIC engine/wind/field.cpp engine/wind/sources.cpp engine/wind/precip.cpp
  engine/wind/storm.cpp engine/wind/lightning.cpp engine/wind/thunder.cpp)
target_link_libraries(ge_wind PUBLIC ge_core)

ge_module(ge_wind EXPORT wind SIMULATION)
