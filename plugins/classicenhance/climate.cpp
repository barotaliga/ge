// The classic levels' air: the table and its lookups.
//
// ge:summary The classic levels' air: the table of climates and hours, and its lookups
// ge:keywords climate level table lookup lava warmth
// ge:skill classicenhance water
// ge:test tests/classicenhance_climate_test.cpp
#include "classicenhance/climate.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "core/libm/libm.hpp"

namespace ge::classicenhance {

namespace {

// An autumn evening's light (sunset 17:30, sunrise 07:00) over every level.
wind::Climate air(double mean_c, double range_c, double humidity, double cloud, double wind,
                  double water_c) {
  wind::Climate c;
  c.mean_c = mean_c;
  c.range_c = range_c;
  c.humidity = humidity;
  c.cloud = cloud;
  c.wind = wind;
  c.water_c = water_c;
  c.sunrise = 7;
  c.sunset = 17.5;
  return c;
}

// The hours run on through one night as the episode goes on.
const LevelAir kLevels[] = {
    {"start", "the hall the episodes are entered from", 19.0, air(6, 7, 0.85, 0.85, 8, 9)},
    {"e1m1", "a base by the water", 20.0, air(2, 6, 0.9, 0.9, 10, 7)},
    {"e1m2", "a castle", 21.0, air(-1, 6, 0.85, 0.9, 11, 5)},
    {"e1m3", "a graveyard and its crypts, warm pools in the cold", 22.5,
     air(3, 5, 0.95, 0.95, 7, 16)},
    {"e1m4", "a grotto of flooded caves", 0.0, air(7, 4, 0.97, 0.95, 7, 9)},
    {"e1m5", "a keep of stone halls in a frost", 1.5, air(-5, 8, 0.75, 0.8, 12, 3)},
    {"e1m6", "a base over lava", 2.5, air(9, 6, 0.6, 0.85, 9, 6)},
    {"e1m7", "the lava halls of the episode's end", 3.5, air(14, 5, 0.45, 0.85, 8, 8)},
    {"e1m8", "the secret level, open to a cold void", 4.5, air(-14, 6, 0.6, 0.8, 12, 2)},
};

// A level nobody named: a cold, damp, windy night under low cloud.
const LevelAir kFallback{"", "a level the table does not hold", 22.0, air(3, 6, 0.85, 0.9, 9, 7)};

std::string key_of(std::string_view map) {
  std::string k(map);
  std::transform(k.begin(), k.end(), k.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (k.rfind("maps/", 0) == 0) k.erase(0, 5);
  if (k.size() > 4 && k.compare(k.size() - 4, 4, ".bsp") == 0) k.erase(k.size() - 4);
  return k;
}

}  // namespace

std::span<const LevelAir> level_airs() { return kLevels; }

const LevelAir* level_air(std::string_view map) {
  const std::string k = key_of(map);
  for (const LevelAir& l : kLevels) {
    if (l.map == k) return &l;
  }
  return nullptr;
}

const LevelAir& fallback_air() { return kFallback; }

double lava_warmth(double distance_m) {
  return kLavaWarmth * core::libm::exp(-std::max(distance_m, 0.0) / kLavaReach);
}

}  // namespace ge::classicenhance
