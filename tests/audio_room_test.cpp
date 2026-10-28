// engine/audio/room.hpp: the fan covers the sphere evenly; a closed box's probe reads its mean
// free path, volume and Eyring decay near the box's own; a bigger room rings longer and a softer
// one shorter; a roofless court lets out its open share and keeps that much outdoor echo; an
// empty world is all open; the early walls come nearest first, a wall on the right heard on
// the right; and the probe is the same numbers twice.
//
// ge:summary Tests for the room probe: the fan, a box's acoustics, open courts, the early walls
// ge:keywords test audio room probe rt60 eyring mean-free-path open-share early-reflections
// ge:skill audio testing
#include <cmath>
#include <string>

#include "audio/room.hpp"
#include "check.hpp"

namespace {

using namespace ge::audio;

// An axis-aligned room lo..hi seen from inside: every ray leaves through a wall, the floor or
// the ceiling; `roof` false leaves the ceiling open.
struct BoxRoom {
  double lo[3], hi[3];
  double absorption = 0.05;
  bool roof = true;
};

RayHit box_ray(const void* ctx, const double from[3], const double dir[3], double reach) {
  const auto& b = *static_cast<const BoxRoom*>(ctx);
  double best = reach;
  int axis = -1;
  for (int k = 0; k < 3; ++k) {
    if (dir[k] == 0) continue;
    const double wall = dir[k] > 0 ? b.hi[k] : b.lo[k];
    const double t = (wall - from[k]) / dir[k];
    if (t > 0 && t < best) {
      best = t;
      axis = k;
    }
  }
  if (axis < 0 || (!b.roof && axis == 1 && dir[1] > 0)) return {};
  RayHit h{true, best, b.absorption, {0, 0, 0}};
  h.normal[axis] = dir[axis] > 0 ? -1 : 1;
  return h;
}

RoomWorld world_of(const BoxRoom& b) { return {&b, box_ray}; }

double eyring(const BoxRoom& b) {
  const double x = b.hi[0] - b.lo[0], y = b.hi[1] - b.lo[1], z = b.hi[2] - b.lo[2];
  const double v = x * y * z, s = 2 * (x * y + y * z + x * z);
  return 0.161 * v / (-s * std::log(1 - b.absorption));
}

RoomProbe probe_in(const BoxRoom& b) {
  const double ear[3] = {(b.lo[0] + b.hi[0]) / 2, b.lo[1] + 1.7, (b.lo[2] + b.hi[2]) / 2};
  return probe_room(world_of(b), ear);
}

}  // namespace

TEST(the_fan_covers_the_sphere_evenly) {
  double mean[3] = {0, 0, 0};
  for (int i = 0; i < kProbeRays; ++i) {
    const auto& d = probe_direction(i);
    CHECK(std::fabs(d[0] * d[0] + d[1] * d[1] + d[2] * d[2] - 1) < 1e-12);
    for (int k = 0; k < 3; ++k) mean[k] += d[k] / kProbeRays;
  }
  for (const double m : mean) CHECK(std::fabs(m) < 0.02);
}

TEST(a_closed_box_reads_near_its_own_acoustics) {
  for (const BoxRoom& b : {BoxRoom{{0, 0, 0}, {5, 3, 4}}, BoxRoom{{0, 0, 0}, {20, 8, 12}},
                           BoxRoom{{0, 0, 0}, {40, 18, 60}, 0.08}}) {
    const RoomProbe p = probe_in(b);
    const double x = b.hi[0] - b.lo[0], y = b.hi[1] - b.lo[1], z = b.hi[2] - b.lo[2];
    const double v = x * y * z, s = 2 * (x * y + y * z + x * z);
    CHECK(p.hits == kProbeRays && p.open_share == 0);
    CHECK_MSG(
        std::fabs(p.mean_free_path_m / (4 * v / s) - 1) < 0.35,
        "mfp " + std::to_string(p.mean_free_path_m) + " against " + std::to_string(4 * v / s));
    CHECK_MSG(std::fabs(p.volume_m3 / v - 1) < 0.4, "volume " + std::to_string(p.volume_m3));
    CHECK_MSG(std::fabs(p.rt60_s / eyring(b) - 1) < 0.35,
              "rt60 " + std::to_string(p.rt60_s) + " against " + std::to_string(eyring(b)));
  }
}

TEST(a_bigger_room_rings_longer_and_a_softer_one_shorter) {
  const RoomProbe small = probe_in({{0, 0, 0}, {4, 3, 4}});
  const RoomProbe big = probe_in({{0, 0, 0}, {30, 12, 30}});
  const RoomProbe soft = probe_in({{0, 0, 0}, {30, 12, 30}, 0.4});
  CHECK(big.rt60_s > 3 * small.rt60_s);
  CHECK(soft.rt60_s < big.rt60_s / 4);
  const double right[3] = {1, 0, 0};
  CHECK(room_sound(big, right).decay_s > room_sound(small, right).decay_s);
}

TEST(a_roofless_court_lets_its_sky_out) {
  BoxRoom court{{0, 0, 0}, {16, 6, 16}};
  court.roof = false;
  const RoomProbe open = probe_in(court);
  const RoomProbe hall = probe_in({{0, 0, 0}, {16, 6, 16}});
  // The sky over the walls: 28 degrees up from the ear at the middle and higher, about a quarter.
  CHECK(open.open_share > 0.15 && open.open_share < 0.4);
  CHECK(open.rt60_s < hall.rt60_s / 5);
  const double right[3] = {1, 0, 0};
  const RoomSound s = room_sound(open, right);
  CHECK(std::fabs(s.echo - open.open_share) < 1e-6);
  CHECK(room_sound(hall, right).echo == 0);
}

TEST(an_empty_world_is_all_open) {
  const RoomWorld none{nullptr,
                       [](const void*, const double*, const double*, double) { return RayHit{}; }};
  const double ear[3] = {0, 0, 0}, right[3] = {1, 0, 0};
  const RoomProbe p = probe_room(none, ear);
  CHECK(p.hits == 0 && p.open_share == 1);
  const RoomSound s = room_sound(p, right);
  CHECK(s.wet == 0 && s.taps == 0 && s.echo == 1);
}

TEST(the_early_walls_come_nearest_first_from_their_side) {
  // Far from the left wall, near the right one.
  const BoxRoom b{{0, 0, 0}, {10, 4, 10}};
  const double ear[3] = {9, 1.7, 5}, right[3] = {1, 0, 0};
  const RoomProbe p = probe_room(world_of(b), ear);
  CHECK(p.walls == kEarlyTaps);
  for (int k = 1; k < p.walls; ++k) CHECK(p.wall_m[k] >= p.wall_m[k - 1]);
  const RoomSound s = room_sound(p, right);
  // The first answers (the right wall and the floor near it) lean right; none comes from the
  // far left wall sooner than one off the right.
  CHECK(s.tap[0].pan > 0);
  CHECK(s.tap[0].delay_ms < s.tap[s.taps - 1].delay_ms);
  CHECK(std::fabs(s.predelay_ms - s.tap[0].delay_ms) < 1e-6f);
}

TEST(the_probe_is_the_same_numbers_twice) {
  const BoxRoom b{{0, 0, 0}, {7, 3, 9}};
  const RoomProbe a = probe_in(b), c = probe_in(b);
  CHECK(a.rt60_s == c.rt60_s && a.mean_free_path_m == c.mean_free_path_m);
  CHECK(a.wall_m == c.wall_m);
}
