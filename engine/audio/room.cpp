// The room probe: the fan, the cast, the acoustics and the RoomSound.
//
// ge:summary The room probe: the fan cast, Eyring's decay, the critical distance, the early walls
// ge:keywords audio room probe fibonacci eyring rt60 critical-distance early-walls room_sound
// ge:skill audio
// ge:test tests/audio_room_test.cpp
#include "audio/room.hpp"

#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::audio {

namespace libm = ge::core::libm;

namespace {

constexpr double kPiD = 3.14159265358979323846;
constexpr double kSabine = 0.161;  // s/m: 24 ln 10 / the speed of sound
constexpr double kMaxRt60 = 12;
constexpr double kMaxWet = 1;  // the tail never outweighs the sound it answers

std::array<std::array<double, 3>, kProbeRays> make_fan() {
  std::array<std::array<double, 3>, kProbeRays> fan{};
  const double golden = kPiD * (3 - std::sqrt(5.0));
  for (int i = 0; i < kProbeRays; ++i) {
    const double y = 1 - 2 * (i + 0.5) / kProbeRays;
    const double r = std::sqrt(1 - y * y);
    const double a = golden * i;
    fan[static_cast<size_t>(i)] = {r * libm::cos(a), y, r * libm::sin(a)};
  }
  return fan;
}

double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

}  // namespace

const std::array<double, 3>& probe_direction(int i) {
  static const std::array<std::array<double, 3>, kProbeRays> fan = make_fan();
  return fan[static_cast<size_t>(i < 0 ? 0 : i >= kProbeRays ? kProbeRays - 1 : i)];
}

RoomProbe probe_room(const RoomWorld& world, const double ear[3], const ProbeSettings& settings) {
  RoomProbe p;
  p.rays = kProbeRays;
  double d2 = 0, d3 = 0, absorb = 0;
  // The nearest hit in each octant round the ear: the early echoes' walls.
  std::array<int, kEarlyTaps> nearest{};
  nearest.fill(-1);
  for (int i = 0; i < kProbeRays; ++i) {
    const std::array<double, 3>& dir = probe_direction(i);
    RayHit h = world.ray ? world.ray(world.ctx, ear, dir.data(), settings.reach_m) : RayHit{};
    if (h.hit && !(h.distance_m < settings.reach_m)) h.hit = false;
    if (h.hit) {
      h.distance_m = h.distance_m < 0.05 ? 0.05 : h.distance_m;
      h.absorption = clampd(h.absorption, 0, 1);
    }
    p.ray[static_cast<size_t>(i)] = h;
    if (!h.hit) continue;
    ++p.hits;
    const double d = h.distance_m;
    // The cone's patch of wall grows as the ray meets it more obliquely (held under 4 times).
    double c = -(dir[0] * h.normal[0] + dir[1] * h.normal[1] + dir[2] * h.normal[2]);
    c = h.normal[0] == 0 && h.normal[1] == 0 && h.normal[2] == 0 ? 1
                                                                 : clampd(c < 0 ? -c : c, 0.25, 1);
    d2 += d * d / c;
    d3 += d * d * d;
    absorb += h.absorption;
    const int octant = (dir[0] > 0 ? 1 : 0) | (dir[1] > 0 ? 2 : 0) | (dir[2] > 0 ? 4 : 0);
    int& best = nearest[static_cast<size_t>(octant)];
    if (best < 0 || d < p.ray[static_cast<size_t>(best)].distance_m) best = i;
  }
  p.open_share = static_cast<double>(p.rays - p.hits) / p.rays;
  if (p.hits == 0) return p;
  p.volume_m3 = 4 * kPiD / 3 * d3 / p.rays;
  p.area_m2 = 4 * kPiD * d2 / p.rays;
  p.mean_free_path_m = 4 * p.volume_m3 / p.area_m2;
  p.absorption = absorb / p.hits;
  p.effective_absorption = clampd((1 - p.open_share) * p.absorption + p.open_share, 0.01, 0.999);
  p.rt60_s = kSabine * (p.mean_free_path_m / 4) / -libm::log(1 - p.effective_absorption);
  p.rt60_s = clampd(p.rt60_s, 0.05, kMaxRt60);
  p.critical_m = 0.057 * std::sqrt(p.volume_m3 / p.rt60_s);
  // The walls, nearest first.
  for (int o = 0; o < kEarlyTaps; ++o) {
    const int i = nearest[static_cast<size_t>(o)];
    if (i < 0) continue;
    int at = p.walls++;
    const double d = p.ray[static_cast<size_t>(i)].distance_m;
    while (at > 0 && p.wall_m[static_cast<size_t>(at - 1)] > d) {
      p.wall_m[static_cast<size_t>(at)] = p.wall_m[static_cast<size_t>(at - 1)];
      p.wall_returns[static_cast<size_t>(at)] = p.wall_returns[static_cast<size_t>(at - 1)];
      p.wall_dir[static_cast<size_t>(at)] = p.wall_dir[static_cast<size_t>(at - 1)];
      --at;
    }
    p.wall_m[static_cast<size_t>(at)] = d;
    p.wall_returns[static_cast<size_t>(at)] = 1 - p.ray[static_cast<size_t>(i)].absorption;
    p.wall_dir[static_cast<size_t>(at)] = probe_direction(i);
  }
  return p;
}

RoomSound room_sound(const RoomProbe& p, const double right[3], const ProbeSettings& settings) {
  RoomSound s;
  s.echo = static_cast<float>(clampd(p.open_share * settings.open_echo, 0, 1));
  if (p.hits == 0) return s;
  s.decay_s = static_cast<float>(p.rt60_s);
  // Past the critical distance the tail outweighs the direct sound; what leaves through the open
  // share never builds a field, so a court keeps part of its tail and an open field almost none.
  const double shut = 1 - p.open_share;
  const double wet = p.critical_m > 0 ? settings.source_m / p.critical_m * shut * shut : 0;
  s.wet = static_cast<float>(clampd(wet, 0, kMaxWet));
  s.damp_hz = static_cast<float>(9000 - 6500 * clampd(p.absorption, 0, 1));
  const double sm = settings.source_m;
  for (int k = 0; k < p.walls; ++k) {
    const size_t i = static_cast<size_t>(k);
    const double path = 2 * p.wall_m[i];
    const double ms = path / kSoundSpeed * 1000;
    if (ms > kMaxEarlyMs) break;
    EarlyTap& t = s.tap[static_cast<size_t>(s.taps++)];
    t.delay_ms = static_cast<float>(ms);
    // An image source behind the wall: the reflected path against the direct one, the surface's
    // share of the pressure returned.
    t.gain = static_cast<float>(sm / std::sqrt(sm * sm + path * path) *
                                std::sqrt(clampd(p.wall_returns[i], 0, 1)));
    const std::array<double, 3>& dir = p.wall_dir[i];
    t.pan = static_cast<float>(
        clampd(dir[0] * right[0] + dir[1] * right[1] + dir[2] * right[2], -1, 1));
    t.damp_hz = static_cast<float>(3000 + 9000 * clampd(p.wall_returns[i], 0, 1));
  }
  s.predelay_ms = s.taps > 0 ? s.tap[0].delay_ms : 10.0f;
  return s;
}

}  // namespace ge::audio
