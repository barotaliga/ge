// A storm's flashes as a frame's lamps and channels (storm_look.hpp).
//
// ge:summary storm_look's definitions: lamps, channels, the frame's choice
// ge:keywords storm lightning lamps bolts storm_lights
// ge:skill mountain
// ge:test tests/mountain_storm_look_test.cpp
#include "mountain/app/storm_look.hpp"

#include <algorithm>

#include "core/math.hpp"
#include "mountain/app/weather_look.hpp"
#include "render/cloud_kinds.hpp"

namespace ge::mountain::app {

namespace {

constexpr int kAlive = 64;           // the flashes a frame's window may list
constexpr double kTauCloud = 12;     // the optical depth out of the cloud: a flash inside it
constexpr double kTauStrike = 3;     // a strike's channel, most of it below the base
constexpr float kWidth = 0.15f;      // m, a channel's true width
constexpr double kFirstLit = 0.004;  // s: the first stroke's window, when the branches show
constexpr double kAboveBase = 200;   // m over the base a strike's channel is still seen

constexpr double kGrownTower = 6000;  // m of tower over the base: a cell grown to the anvil
constexpr double kLeastCover = 0.25, kMostCover = 0.85;  // the towers' cover far off, overhead
constexpr double kSunTaken = 0.95;  // the sun's share taken with a tower overhead
// The storm's slate overhead: its light, its grey overhead over the horizon's, its tint.
constexpr double kStormLight = 0.3, kStormZenith = 0.25;
constexpr double kStormTint[3] = {0.82, 0.9, 1.06};

// A flash's key: its cell and the slot it started in.
uint64_t key_of(const wind::Flash& f) {
  return (static_cast<uint64_t>(f.cell) << 48) ^ static_cast<uint64_t>(static_cast<int64_t>(f.t0));
}

}  // namespace

render::FlashLamp lamp_of(const wind::FlashLight& l) {
  render::FlashLamp lamp;
  lamp.at[0] = l.x;
  lamp.at[1] = l.y;
  lamp.at[2] = l.z;
  lamp.radius_m = l.radius;
  lamp.candela = l.power * wind::kUnitCandela;
  lamp.out_tau = l.cloud ? kTauCloud : kTauStrike;
  return lamp;
}

render::Flashes lamps_of(const wind::Flash* flash, double a, double b) {
  render::Flashes out;
  if (flash == nullptr) return out;
  const wind::FlashLight l = wind::light_of(*flash, a, b);
  if (l.power <= 0) return out;
  out.count = 1;
  out.lamp[0] = lamp_of(l);
  return out;
}

void bolts_of(const wind::Flash& flash, const wind::Channel& ch, double a, double b, double base_y,
              std::vector<render::BoltSegment>& out) {
  if (flash.kind != wind::FlashKind::ground) return;
  const double candela = wind::light_of(flash, a, b).power * wind::kUnitCandela;
  if (candela <= 0) return;
  const double first = flash.t0 + flash.stroke[0].t;
  const bool branches = first < b && first + kFirstLit >= a;
  const size_t from = out.size();
  double sum = 0;
  for (int i = 0; i < ch.count; ++i) {
    const wind::Segment& s = ch.seg[i];
    if (s.order > 0 && !branches) continue;
    if (std::min(s.y0, s.y1) > base_y + kAboveBase) continue;
    const double len = core::hypot(core::hypot(s.x1 - s.x0, s.y1 - s.y0), s.z1 - s.z0);
    sum += s.glow * len;
    out.push_back({{s.x0, s.y0, s.z0}, kWidth, {s.x1, s.y1, s.z1}, s.glow});
  }
  if (sum <= 0) {
    out.resize(from);
    return;
  }
  for (size_t i = from; i < out.size(); ++i)
    out[i].candela = static_cast<float>(out[i].candela * candela / sum);
}

void storm_lights(StormLights& s, const wind::Storm& storm, const double eye[3], double a, double b,
                  bool channels) {
  s.lamps = {};
  s.sky = 0;
  s.bolts.clear();
  s.alive.resize(kAlive);
  const int n = std::min(wind::flashes_between(storm, a, b, s.alive.data(), kAlive), kAlive);
  // A channel kept past its flash is let go.
  std::erase_if(s.kept, [a](const StormLights::Kept& k) { return k.end < a; });
  if (n <= 0) return;
  s.sky = wind::sky_light(s.alive.data(), n, eye[0], eye[1], eye[2], a, b);
  wind::FlashLight lit[render::kFlashLamps];
  s.lamps.count =
      wind::brightest(s.alive.data(), n, eye[0], eye[1], eye[2], a, b, lit, render::kFlashLamps);
  for (int i = 0; i < s.lamps.count; ++i) s.lamps.lamp[i] = lamp_of(lit[i]);
  if (!channels) return;
  const double base_y = storm.ground + storm.base;
  for (int i = 0; i < n; ++i) {
    const wind::Flash& f = s.alive[static_cast<size_t>(i)];
    if (f.kind != wind::FlashKind::ground || wind::light_over(f, a, b) <= 0) continue;
    const uint64_t key = key_of(f);
    auto it = std::find_if(s.kept.begin(), s.kept.end(),
                           [key](const StormLights::Kept& k) { return k.key == key; });
    if (it == s.kept.end()) {
      StormLights::Kept k{key, f.t0 + f.length, std::make_unique<wind::Channel>()};
      wind::build_channel(f, *k.channel);
      s.kept.push_back(std::move(k));
      it = s.kept.end() - 1;
    }
    bolts_of(f, *it->channel, a, b, base_y, s.bolts);
  }
}

double nearness(const wind::Storm& storm, double t, double x, double z) {
  double most = 0;
  for (int k = 0; k < storm.cells; ++k) {
    const wind::CellAt c = wind::cell_at(storm, k, t);
    if (c.stage == wind::Stage::none) continue;
    const double grown = std::min(1.0, (c.top - storm.base) / kGrownTower);
    const double dx = x - c.x, dz = z - c.z;
    const double q = (dx * dx + dz * dz) / (kStormReach * kStormReach);
    most = std::max(most, grown / (1 + q));
  }
  return most;
}

void storm_sky(const mapgen::Level& level, const wind::Storm& storm, double t, const double eye[3],
               bool layers, const Aloft& aloft, render::Weather& weather, render::Clouds& clouds) {
  const double near = nearness(storm, t, eye[0], eye[2]);
  clouds = {};
  if (layers) {
    const render::CloudKind* kinds[render::kCloudLayers] = {
        render::cloud_kind_named("cumulonimbus")};
    // The towers' base is the storm's (over the summit) unless one was picked by hand.
    Aloft over = aloft;
    if (over.base < 0) over.base = storm.base;
    clouds = cloud_layers_of(kinds, 1, level.weather, level.seed, level.mountain.base.y,
                             level.sun.latitude, t, over);
    if (clouds.count > 0) clouds.layers[0].cover = kLeastCover + (kMostCover - kLeastCover) * near;
  }
  const air::Fall fall = air::fall_at(level, storm, t, eye[0], eye[1], eye[2]);
  // The storm's sky is a summer afternoon's in place of the map's own: no lid's damp air or valley
  // fog, the air clear but for the fall in it.
  mapgen::Weather summer = level.weather;
  summer.kind = mapgen::WeatherKind::fair;
  summer.fog = 0;
  weather = weather_look(summer, fall, clouds.count > 0);
  // Under the towers the storm's slate closes over the dome as it nears: the sun gone, the deck's
  // light a storm's and darker overhead, a little blue in its grey.
  weather.cover = std::max(weather.cover, near);
  weather.sun_pass *= 1 - kSunTaken * near;
  weather.deck_light += (kStormLight - weather.deck_light) * near;
  weather.deck_zenith += (kStormZenith - weather.deck_zenith) * near;
  for (int i = 0; i < 3; ++i) weather.deck_tint[i] += (kStormTint[i] - weather.deck_tint[i]) * near;
}

}  // namespace ge::mountain::app
