// GAP JUMPS READ OFF A TRACK (plugins/shooter/gap_jumps.hpp): the move itself flies a jump over
// a pit of lava 320 across at a hopper's speed (read as a speed jump over harm, a run's reach
// short of it), the same jump at a run's speed falls in (no gap jump: it landed in the pit), a
// hop on the flat at the same speed has no gap under it, and a jump inside a blast jump's frames
// is skipped; a jump's reach rises with its speed and falls with the height it comes down to.
//
// ge:summary Tests: gap jumps read off the move's own flights over a lava pit and on the flat
// ge:keywords test gap-jump speed-jump read_gap_jumps jump_reach pit lava track
// ge:skill shooter testing
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/gap_jumps.hpp"
#include "shooter/hull.hpp"
#include "shooter/pmove.hpp"

namespace sh = ge::shooter;

namespace {

sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

// Floors at z 0 west of x 0 and east of x 320; between, a pit floored at -192 with lava to -128.
struct Pit {
  sh::HullData point, clip;
  sh::MoveWorld world;
  Pit() {
    sh::Brush lava = box({0, -1024, -192}, {320, 1024, -128});
    lava.contents = sh::kContentsLava;
    const std::vector<sh::Brush> brushes = {box({-1024, -1024, -64}, {0, 1024, 0}),
                                            box({320, -1024, -64}, {1344, 1024, 0}),
                                            box({-1024, -1024, -256}, {1344, 1024, -192}), lava};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {-1024, -1024, -256};
    world.solids[0].maxs = {1344, 1024, 0};
    world.count = 1;
  }
};

// The move's own run east from `x` at `speed`: two commands on the ground, then the jump, the
// forward key held; a point a command for 1.5 s, as a recording would hold it.
std::vector<sh::TrackPoint> flown(const sh::MoveWorld& world, float x, float speed) {
  sh::MoveState s;
  s.origin = {x, 0, 24};
  s.velocity = {speed, 0, 0};
  const sh::MoveSettings set;
  s.maxspeed = set.maxspeed;
  std::vector<sh::TrackPoint> track;
  constexpr int kMsec = 13;
  for (int i = 0; i < 115; i++) {
    track.push_back({i * kMsec * 0.001, s.origin, 0, 0, false});
    sh::MoveCmd c;
    c.msec = kMsec;
    c.forward = 320;
    c.buttons = i == 1 ? sh::kButtonJump : 0;
    sh::player_move(s, c, set, world);
  }
  return track;
}

std::vector<sh::GapJump> read(const sh::MoveWorld& world, const std::vector<sh::TrackPoint>& tr,
                              std::vector<std::pair<int32_t, int32_t>> blasts = {}) {
  sh::HopReading hops;
  sh::read_hops(tr, world, sh::HopSettings{}, hops);
  std::vector<sh::GapJump> out;
  sh::read_gap_jumps(tr, hops, world, {}, blasts, sh::GapJumpReadSettings{}, out);
  return out;
}

std::string about(const std::vector<sh::GapJump>& js) {
  std::string s = std::to_string(js.size()) + " gap jumps";
  for (const sh::GapJump& j : js)
    s += "; from " + std::to_string(j.from.x) + " to " + std::to_string(j.to.x) + " z " +
         std::to_string(j.to.z) + " speed " + std::to_string(j.speed) + " across " +
         std::to_string(j.across) + " reach " + std::to_string(j.run_reach) + " depth " +
         std::to_string(j.depth);
  return s;
}

}  // namespace

TEST(a_jump_over_the_lava_at_a_hoppers_speed_is_a_speed_jump_over_harm) {
  const Pit pit;
  const std::vector<sh::GapJump> js = read(pit.world, flown(pit.world, -10, 560));
  CHECK_MSG(js.size() == 1, about(js));
  if (js.size() != 1) return;
  const sh::GapJump& j = js[0];
  CHECK_MSG(j.from.x > -10 && j.from.x < 16 && j.to.x > 304, about(js));
  CHECK_MSG(j.to.z > 23 && j.to.z < 25, about(js));  // landed on the east floor
  CHECK_MSG(j.speed > 450, about(js));
  CHECK_MSG(j.harm && j.depth > 150, about(js));
  CHECK_MSG(j.speed_needed && j.run_reach < j.across, about(js));
}

TEST(the_same_jump_at_a_runs_speed_falls_in_and_is_no_gap_jump) {
  const Pit pit;
  const std::vector<sh::TrackPoint> tr = flown(pit.world, -10, 340);
  CHECK_MSG(tr.back().origin.z < -100, "the run's jump should fall into the pit");
  const std::vector<sh::GapJump> js = read(pit.world, tr);
  CHECK_MSG(js.empty(), about(js));
}

TEST(a_hop_on_the_flat_has_no_gap_under_it) {
  const Pit pit;
  const std::vector<sh::GapJump> js = read(pit.world, flown(pit.world, -900, 560));
  CHECK_MSG(js.empty(), about(js));
}

TEST(a_jump_inside_a_blast_jumps_frames_is_skipped) {
  const Pit pit;
  const std::vector<sh::GapJump> js = read(pit.world, flown(pit.world, -10, 560), {{1, 60}});
  CHECK_MSG(js.empty(), about(js));
}

TEST(a_jumps_reach_rises_with_speed_and_falls_with_the_height_it_comes_down_to) {
  const float level = sh::jump_reach(320, 0, 270, 800);
  CHECK(level > 215 && level < 217);  // 320 x 0.675 s
  CHECK(sh::jump_reach(480, 0, 270, 800) > level);
  CHECK(sh::jump_reach(320, -96, 270, 800) > level);  // a jump down flies longer
  CHECK(sh::jump_reach(320, 64, 270, 800) == 0);      // a jump never rises 64
}
