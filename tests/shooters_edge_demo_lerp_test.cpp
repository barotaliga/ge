// A demo drawn between its frames (games/shooters_edge/client/demo_lerp.hpp): angles the shortest
// way round, each rule that draws a player or an entity at the newer frame (a teleport, a respawn,
// the first gibbed frame, another model, a gap), and a written multiview match played in the
// window's paced run: the walker drawn on his straight line at the clock's time, evenly spaced
// between the recording's frames, the teleporter not dragged across the map, the eye riding the
// tracked player's drawn origin, and `demo_lerp 0` drawing each frame as read.
//
// ge:summary Tests: a demo drawn between its frames, the snapping rules, the paced run smooth
// ge:keywords test demo lerp interpolation smooth teleport respawn gib shortest-angle playdemo
// ge:skill shooter testing
// ge:test tests/shooters_edge_demo_lerp_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo_lerp.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/timedemo.hpp"

namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

namespace {

bool near(float a, float b, float tol = 1e-3f) { return std::fabs(a - b) <= tol; }

cl::DemoEntity body(sh::Vec3f at) {
  cl::DemoEntity e;
  e.origin = at;
  e.model = 1;
  e.present = true;
  return e;
}

// Four players for 2 s at 20 frames a second: slot 0 walks +x 16 units a frame (320 u/s), slot 1
// stands, slot 2 walks and is teleported 1000 units at frame 20, slot 3 dies at frame 10 and comes
// back 400 units away at frame 30.
cl::MatchScript walk_match() {
  cl::MatchScript m;
  m.frame_s = 0.05f;
  m.frames = 40;
  m.players.resize(4);
  const char* names[] = {"walker", "stander", "porter", "dier"};
  for (int k = 0; k < 4; k++) m.players[static_cast<size_t>(k)].name = names[k];
  for (int f = 0; f < m.frames; f++) {
    const float x = static_cast<float>(f) * 16;
    m.players[0].origins.push_back({x, 0, 24});
    m.players[1].origins.push_back({0, 256, 24});
    m.players[2].origins.push_back({f < 20 ? x : 1000 + x, -256, 24});
    m.players[3].origins.push_back({f < 30 ? 64.0f : 464.0f, 512, 24});
    m.players[3].health.push_back(f >= 10 && f < 30 ? 0 : 100);
  }
  return m;
}

cl::Timedemo opened(bool smooth) {
  cl::Timedemo t;
  std::string error;
  const bool ok = cl::open_demo(cl::write_multiview_match(walk_match()), t.demo, error);
  CHECK_MSG(ok, error.c_str());
  t.start = t.demo;
  t.active = true;
  t.paced = true;
  t.smooth = smooth;
  t.demo.track = 0;
  return t;
}

}  // namespace

TEST(angles_turn_the_shortest_way_round) {
  CHECK(near(cl::lerp_angle(350, 10, 0.5f), 360));
  CHECK(near(cl::lerp_angle(10, 350, 0.5f), 0));
  CHECK(near(cl::lerp_angle(0, 90, 0.25f), 22.5f));
  CHECK(near(cl::lerp_angle(-170, 170, 0.5f), -180));
  CHECK(near(cl::lerp_angle(0, 720 + 30, 1), 30));  // whole turns are not turned through
  CHECK(near(cl::lerp_angle(0, 180, 0.5f), -90));   // exactly opposite: the other way round
}

TEST(a_player_is_drawn_between_unless_a_rule_snaps_him) {
  cl::DemoEntity out;
  cl::DemoEntity a = body({0, 0, 0}), b = body({100, 0, 0});
  a.angles = {0, 350, 0};
  b.angles = {0, 10, 0};
  CHECK(cl::lerp_player(a, b, 0.25f, true, out));
  CHECK(near(out.origin.x, 25) && near(out.angles.y, 355));
  CHECK(cl::lerp_player(a, b, 0.25f, false, out));  // his view was set: the angles jump
  CHECK(near(out.origin.x, 25) && near(out.angles.y, 10));
  b.origin = {151, 0, 0};  // a teleport
  CHECK(!cl::lerp_player(a, b, 0.5f, true, out) && near(out.origin.x, 151));
  b.origin = {10, 0, 0};
  a.dead = true;  // a respawn
  CHECK(!cl::lerp_player(a, b, 0.5f, true, out) && near(out.origin.x, 10));
  a.dead = false;
  b.gib = true;  // the first gibbed frame
  CHECK(!cl::lerp_player(a, b, 0.5f, true, out));
  a.dead = true;  // gibbed as a corpse: drawn between
  b.dead = true;
  CHECK(cl::lerp_player(a, b, 0.5f, true, out) && near(out.origin.x, 5));
  a.present = false;  // missing from the older frame
  CHECK(!cl::lerp_player(a, b, 0.5f, true, out) && near(out.origin.x, 10));
}

TEST(an_entity_is_drawn_between_unless_a_rule_snaps_it) {
  cl::DemoEntity out;
  cl::DemoEntity a = body({0, 0, 0}), b = body({0, 0, 200});
  CHECK(cl::lerp_entity(a, b, 0.5f, out) && near(out.origin.z, 100));
  b.origin.z = 201;
  CHECK(!cl::lerp_entity(a, b, 0.5f, out) && near(out.origin.z, 201));
  b.origin.z = 50;
  b.model = 2;  // another model: a new entity in the old one's number
  CHECK(!cl::lerp_entity(a, b, 0.5f, out) && near(out.origin.z, 50));
}

TEST(the_fraction_holds_to_the_two_frames_and_stops_at_a_gap) {
  cl::DemoLerp l;
  cl::DemoState s;
  s.frames = 2;
  s.time = 1.0;
  cl::lerp_frame(l, s);
  s.time = 1.05;
  cl::lerp_frame(l, s);
  CHECK(std::fabs(cl::lerp_fraction(l, 1.0125) - 0.25) < 1e-9);
  CHECK(cl::lerp_fraction(l, 0.5) == 0 && cl::lerp_fraction(l, 2) == 1);
  s.time = 1.5;  // a stall: not drawn across
  cl::lerp_frame(l, s);
  CHECK(cl::lerp_fraction(l, 1.2) == 1);
  s.time = 1.55;
  cl::lerp_frame(l, s, true);  // a seek
  CHECK(cl::lerp_fraction(l, 1.52) == 1);
}

TEST(a_paced_match_is_drawn_smoothly_between_its_frames) {
  cl::Timedemo t = opened(true);
  std::string error;
  int pictures = 0, between = 0;
  float last_x = -1;
  double last_clock = -1;
  float most_step = 0, least_step = 1e9f;
  for (int i = 0; i < 2000; i++) {
    // 80 pictures a second against the recording's 20 frames: four a frame.
    const cl::TimedemoStep st = cl::playdemo_step(t, 0.0125, 1, error);
    if (st == cl::TimedemoStep::end) break;
    CHECK_MSG(st != cl::TimedemoStep::error, error.c_str());
    const cl::DemoState& d = cl::demo_drawn(t);
    if (d.players.size() < 4 || !d.players[0].present) continue;
    pictures++;
    if (t.lerp.f < 1) between++;
    const float x = d.players[0].origin.x;
    // The walker stands on his line at the clock's time (320 u/s from the match's first frame).
    const double want = t.lerp.to.players[0].origin.x - 320.0 * (t.lerp.to.time - t.clock);
    CHECK_MSG(
        std::fabs(want - x) < 0.6 || t.lerp.f >= 1,
        ("the walker at x " + std::to_string(x) + " clock " + std::to_string(t.clock)).c_str());
    // The eye rides his drawn origin.
    CHECK(near(d.eye.x, x) && near(d.eye.z, d.players[0].origin.z + d.view_height));
    if (last_x >= 0 && t.clock > last_clock && x > last_x) {
      most_step = std::max(most_step, x - last_x);
      least_step = std::min(least_step, x - last_x);
    }
    last_x = x;
    last_clock = t.clock;
    // The teleporter is never drawn halfway across the map, nor the dier between his body and
    // where he comes back.
    const float px = d.players[2].origin.x;
    CHECK_MSG(px < 320 || px >= 1300, ("the teleporter at x " + std::to_string(px)).c_str());
    const float dx = d.players[3].origin.x;
    CHECK_MSG(near(dx, 64) || near(dx, 464), ("the dier at x " + std::to_string(dx)).c_str());
  }
  CHECK(pictures > 100);
  CHECK(between > pictures / 2);
  // Even steps: every picture moves the walker a quarter of a frame's 16 units.
  CHECK_MSG(
      most_step - least_step < 0.6f,
      ("steps from " + std::to_string(least_step) + " to " + std::to_string(most_step)).c_str());
}

TEST(demo_lerp_off_draws_each_frame_as_read) {
  cl::Timedemo t = opened(false);
  std::string error;
  for (int i = 0; i < 60; i++) {
    const cl::TimedemoStep st = cl::playdemo_step(t, 0.0125, 1, error);
    CHECK(st != cl::TimedemoStep::error);
    const cl::DemoState& d = cl::demo_drawn(t);
    CHECK(&d == &t.demo.state);
    if (!d.players.empty() && d.players[0].present)
      CHECK(std::fmod(d.players[0].origin.x, 16.0f) == 0);  // on a frame's place
  }
}
