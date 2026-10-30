# The wind (engine/wind): a pure function of place and time over a field built once, the
# mountain's air one configuration of it (docs/water.md § Wind), and what falls through it (snow,
# sleet and rain: engine/wind/precip.hpp), the thunderstorms in it (storm, lightning, thunder:
# docs/storms.md), and the air's temperature and moisture with what they do: fog, frost, ice, the
# snow's feel (climate.hpp, mist.hpp, cold.hpp; docs/climate.md). Deterministic, no platform;
# published as ge::wind.
add_library(ge_wind STATIC engine/wind/field.cpp engine/wind/sources.cpp engine/wind/precip.cpp
                    engine/wind/storm.cpp engine/wind/lightning.cpp engine/wind/thunder.cpp engine/wind/thunder_play.cpp
                    engine/wind/climate.cpp engine/wind/mist.cpp engine/wind/cold.cpp
                    engine/wind/aloft.cpp)
target_link_libraries(ge_wind PUBLIC ge_core ge_audio)

ge_module(ge_wind EXPORT wind SIMULATION)
