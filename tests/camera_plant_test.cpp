// Planted lenses where the path is known (engine/camera/plant.hpp): the sight line closes on a
// hill and a trunk, a lens planted beside a path steps round a trunk that would hide the moment,
// the replay's planted aim is a pure function of the path and the step, and the broadcast keeps
// its lens the least hold, hands over to the lens the subject runs toward, and cuts at once on a
// lost line.
//
// ge:summary Tests: sight through the ground and trunks, planting round a trunk, pure aims, cuts
// ge:keywords test camera planted trackside broadcast sight plant replay film cut
// ge:skill camera testing
// ge:test tests/camera_plant_test.cpp
#include <cmath>
#include <vector>

#include "camera/plant.hpp"
#include "check.hpp"
#include "core/math.hpp"

namespace cam = ge::camera;

namespace {

// A flat world with a ridge at z in [40, 44] 6 m high and the trunks below as obstacles.
struct Scene {
  std::vector<cam::Obstacle> trunks;
  bool ridge = false;
};

double ground(const void* ctx, double, double z) {
  const auto* s = static_cast<const Scene*>(ctx);
  return s->ridge && z >= 40 && z <= 44 ? 6.0 : 0.0;
}

int trunks(const void* ctx, double x, double z, double reach, cam::Obstacle* out, int most) {
  const auto* s = static_cast<const Scene*>(ctx);
  int n = 0;
  for (const cam::Obstacle& o : s->trunks) {
    if (n >= most) break;
    if (ge::core::hypot(o.x - x, o.z - z) <= reach + o.radius) out[n++] = o;
  }
  return n;
}

cam::World world_of(const Scene& s) {
  cam::World w;
  w.ctx = &s;
  w.ground = ground;
  w.obstacles = trunks;
  return w;
}

// Straight down +z at 10 m/s from z = 0, 120 points a second for 4 s.
std::vector<cam::PathPoint> straight() {
  std::vector<cam::PathPoint> p;
  for (int i = 0; i <= 480; ++i) p.push_back({{0, 0, i / 12.0}, {0, 0, 10}});
  return p;
}

}  // namespace

TEST(the_sight_line_closes_on_a_ridge_and_a_trunk) {
  Scene s;
  s.ridge = true;
  s.trunks.push_back({10, 0, 0.5, 0, 12, 1});
  const cam::World w = world_of(s);
  // Over the ridge low: closed near its face.
  const double r = cam::sight_share(w, {0, 1, 30}, {0, 1, 60});
  CHECK(r > 0.3 && r < 0.4);
  // High over it: open.
  CHECK(cam::sight_share(w, {0, 20, 30}, {0, 20, 60}) == 1);
  // Through the trunk: closed at its middle.
  const double t = cam::sight_share(w, {0, 1, 0}, {20, 1, 0});
  CHECK(t > 0.45 && t <= 0.5);
  // Beside it: open.
  CHECK(cam::sight_share(w, {0, 1, 3}, {20, 1, 3}) == 1);
}

TEST(a_lens_planted_beside_a_path_steps_round_a_trunk_that_would_hide_it) {
  const std::vector<cam::PathPoint> path = straight();
  Scene open;
  cam::PlantSpec spec;
  spec.key = 240;  // z = 20
  spec.across_m = 14;
  spec.sides = 1;  // the right of travel along +z is +x
  const cam::Planting a = cam::plant_beside(path, spec, world_of(open));
  CHECK(a.candidate == 0);  // nothing in the way: the spec's own place
  CHECK(std::fabs(a.row.eye.x - 14) < 1e-9 && std::fabs(a.row.eye.z - 20) < 1e-9);
  CHECK(a.row.eye.y == spec.height_m && a.seen == 1);
  // A trunk between that place and the path: the lens moves to a place that sees.
  Scene wood;
  wood.trunks.push_back({7, 20, 2.5, -1, 30, 1});
  const cam::Planting b = cam::plant_beside(path, spec, world_of(wood));
  int own = 0, tested = 0;
  for (size_t i = 0; i < path.size(); i += static_cast<size_t>(spec.every), ++tested) {
    const cam::Vec3 aim{path[i].pos.x, path[i].pos.y + spec.aim_height_m, path[i].pos.z};
    own += cam::sight_share(world_of(wood), a.row.eye, aim) >= 0.999 ? 1 : 0;
  }
  const double own_seen = static_cast<double>(own) / tested;
  CHECK(own_seen < 0.8);
  CHECK(b.candidate > 0 && b.seen > own_seen + 0.1);
  // The same path and world always plant the same lens.
  const cam::Planting c = cam::plant_beside(path, spec, world_of(wood));
  CHECK(c.candidate == b.candidate && c.row.eye.x == b.row.eye.x && c.row.eye.z == b.row.eye.z);
}

TEST(the_replays_planted_aim_is_pure_and_smooth) {
  std::vector<cam::PathPoint> path = straight();
  // A one-step jolt in the path: the window spreads it over 61 steps.
  path[200].pos.x = 0.6;
  cam::PlantedRow row;
  row.eye = {-14, 2, 20};
  row.hold_m = 3;
  const cam::Pose a = cam::planted_at(row, path, 200, 30);
  const cam::Pose b = cam::planted_at(row, path, 200, 30);
  CHECK(a.yaw == b.yaw && a.pitch == b.pitch && a.fov_y == b.fov_y);
  const cam::Pose c = cam::planted_at(row, path, 199, 30);
  CHECK(std::fabs(ge::core::angle_diff(a.yaw, c.yaw)) < 0.01);
  const cam::Vec3 m = cam::smoothed_at(path, 200, 30);
  CHECK(std::fabs(m.x - 0.6 / 61) < 1e-12);
  // Nearer, the zoom opens: the subject's height in frame is held.
  const cam::Pose near = cam::planted_at(row, path, 240, 30);
  const cam::Pose far = cam::planted_at(row, path, 480, 30);
  CHECK(near.fov_y > far.fov_y);
}

TEST(the_broadcast_holds_hands_over_toward_and_cuts_on_a_lost_line) {
  Scene s;
  const cam::World w = world_of(s);
  // Three lenses down +z, alternating sides.
  std::vector<cam::PlantedRow> lenses(3);
  lenses[0].eye = {-15, 3, 20};
  lenses[1].eye = {15, 3, 120};
  lenses[2].eye = {-15, 3, 220};
  cam::BroadcastRule rule;
  cam::BroadcastState st;
  cam::Subject sub;
  sub.vel = {0, 0, 10};
  std::vector<int> live;
  for (int i = 0; i <= 2400; ++i) {  // 20 s at 120 Hz: z from 0 to 200
    sub.pos = {0, 0, i / 12.0};
    live.push_back(cam::direct_broadcast(lenses, sub, w, rule, st, 1.0 / 120));
  }
  CHECK(live.front() == 0 && live.back() == 2);
  CHECK(st.cuts == 2);
  // No cut before the least hold: every run of one lens is at least min_hold_s long.
  int run = 0;
  for (size_t i = 1; i < live.size(); ++i) {
    if (live[i] != live[i - 1]) {
      CHECK(run >= static_cast<int>(rule.min_hold_s * 120));
      run = 0;
    }
    ++run;
  }
  // A trunk across the live lens's line cuts at once, held or not.
  Scene wood;
  wood.trunks.push_back({-7, 11.5, 1.5, -1, 30, 1});
  cam::BroadcastState fresh;
  sub.pos = {0, 0, 0};
  CHECK(cam::direct_broadcast(lenses, sub, w, rule, fresh, 1.0 / 120) == 0);
  const cam::World blocked = world_of(wood);
  sub.pos = {0, 0, 4};
  int after = 0;
  for (int i = 0; i < 60; ++i)
    after = cam::direct_broadcast(lenses, sub, blocked, rule, fresh, 1.0 / 120);
  CHECK(after != 0 && fresh.cuts == 1);
}
