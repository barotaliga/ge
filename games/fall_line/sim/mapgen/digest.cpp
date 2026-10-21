// Port of game5 `engine/mapgen/digest.ts`.
//
// ge:summary levelDigest's mixing order, word for word with the oracle
// ge:keywords digest fnv levelDigest
// ge:oracle game5/engine/mapgen/digest.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#include "mapgen/digest.hpp"

#include <cstdio>
#include <string_view>

#include "core/js.hpp"
#include "mapgen/grades.hpp"
#include "mapgen/regions.hpp"
#include "mapgen/weather.hpp"

namespace fall_line::mapgen {

namespace js = ge::core::js;

namespace {

// Stations between two the digest reads.
constexpr size_t kTrackStride = 10;

struct Fnv {
  uint32_t hash = 0x811c9dc5u;
  void byte(uint32_t b) { hash = (hash ^ (b & 0xffu)) * 0x01000193u; }
  // Four bytes of the centimetre-rounded value.
  void mix(double v) {
    const auto n = static_cast<uint32_t>(js::to_int32(js::round(v * 100)));
    for (int shift = 0; shift < 32; shift += 8) byte(n >> shift);
  }
  void word(std::string_view s) {
    for (const char c : s) byte(uint8_t(c));
  }
};

}  // namespace

std::string level_digest(const Level& level) {
  Fnv f;
  f.mix(level.size);
  f.mix(level.laps);
  f.mix(level.track.length);
  const auto& points = level.track.points;
  f.mix(double(points.size()));
  for (size_t i = 0; i < points.size(); i += kTrackStride) {
    const TrackPoint& p = points[i];
    f.mix(p.x);
    f.mix(p.z);
    f.mix(p.y);
    f.mix(p.width);
  }
  for (const Checkpoint& c : level.checkpoints) {
    f.mix(c.x);
    f.mix(c.z);
    f.mix(c.heading);
    f.mix(c.width);
    f.mix(c.s);
    f.mix(level.ground_at(c.x, c.z));
  }
  for (const Spawn& g : level.grid) {
    f.mix(g.x);
    f.mix(g.z);
    f.mix(g.heading);
  }
  for (const Kicker& k : level.kickers) {
    f.word(k.id);
    f.mix(k.x);
    f.mix(k.z);
    f.mix(k.heading);
    f.mix(k.height);
    f.mix(k.ramp);
    f.mix(k.landing);
    f.mix(k.width);
    f.mix(level.ground_at(k.x, k.z));
  }
  for (const Cliff& c : level.cliffs) {
    f.word(c.id);
    f.mix(c.x);
    f.mix(c.z);
    f.mix(c.heading);
    f.mix(c.drop);
    f.mix(level.ground_at(c.x, c.z));
  }
  for (const Drift& d : level.drifts) {
    f.mix(d.from);
    f.mix(d.to);
  }
  f.mix(double(level.trees.size()));
  for (const TreeDef& t : level.trees) {
    f.mix(t.x);
    f.mix(t.z);
    f.mix(t.height);
  }
  f.mix(level.sun.hour);
  f.mix(level.sun.day_of_year);
  f.mix(level.sun.latitude);
  if (level.sun.facing) f.mix(*level.sun.facing);
  const Weather& sky = level.weather;
  f.word(weather_name(sky.kind));
  f.mix(sky.snowfall);
  f.mix(sky.fog);
  f.mix(sky.wind);
  f.mix(sky.wind_from);
  f.word(sky.evening ? "evening" : "day");
  if (level.region != RegionId::alpine) f.word(region_name(level.region));
  if (level.grade) f.word(grade_name(*level.grade));
  char out[9];
  std::snprintf(out, sizeof out, "%08x", f.hash);
  return out;
}

}  // namespace fall_line::mapgen
