// THE CLASSIC LEVELS' AIR (docs/classic-enhance.md § The air, docs/climate.md): a day's
// `wind::Climate` for every level of the classic pack and the hour it is played at, so the engine's
// temperature and humidity (engine/wind/climate.hpp) and what they do (the fog, snow or rain, the
// breath, frost, the steam off a pool) apply to the classic levels as to any world.
//
// Every level is played in the EVENING or at NIGHT, in a WIND, under LOW CLOUD (the classic sky's
// look): the hours run on through one night as the episode goes on, the wind 7 to 12 m/s, the
// cloud 0.8 to 0.95, which keeps radiation fog off the open yards (the wind mixes it away) while a
// pool far warmer than a cold hall's air still steams. Each level's temperature, humidity and
// pools' temperature follow what the level is: a base by cold water, a castle and its moat, a
// graveyard by warm slime, a dripping grotto, a keep in a frost, halls warmed by lava, a vault
// open to the cold. The pools' temperature is `Climate::water_c` (slime and lava pools are warm;
// a site by lava adds `kLavaWarmth` as `Site::warmth`, fading with the distance).
//
// The table is this repository's own numbers; nothing in it is read from the pack. Distances a
// renderer hands a `Site` are metres: a classic level's units are about 1/32 m (`kMetresPerUnit`).
//
// ge:summary The classic levels' air: a Climate and an hour for every level of the classic pack
// ge:keywords climate temperature humidity level evening night wind low-cloud steam breath fog
// ge:keywords level_air level_airs lava_warmth kMetresPerUnit
// ge:skill classicenhance water
// ge:test tests/classicenhance_climate_test.cpp
#pragma once

#include <span>
#include <string_view>

#include "wind/climate.hpp"

namespace ge::classicenhance {

inline constexpr double kMetresPerUnit = 1.0 / 32;  // a classic level's unit, m
inline constexpr double kLavaWarmth = 12;           // K a lava pool adds to the air at its edge
inline constexpr double kLavaReach = 6;             // m: the warmth's fade (1/e)

// A level's air: its map's name (`maps/<map>.bsp`), what the level is (in this repository's
// words), the solar hour it is played at and its day's climate.
struct LevelAir {
  std::string_view map;
  std::string_view what;
  double hour = 21;
  wind::Climate climate;
};

// Every level of the classic pack, in the order they are played.
std::span<const LevelAir> level_airs();

// The level `map`'s air (any case; "maps/" and ".bsp" may be given), or null for a map the table
// does not hold. A level the table does not hold is played under `fallback_air()`.
const LevelAir* level_air(std::string_view map);
const LevelAir& fallback_air();

// The warmth, K, a lava pool `distance_m` away adds to the air (`Site::warmth`).
double lava_warmth(double distance_m);

}  // namespace ge::classicenhance
