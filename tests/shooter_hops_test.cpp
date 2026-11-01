// THE HOP READER: on a flat floor an ideal hopper (the side key alone, the view on the velocity,
// the jump on the command he lands, the side switched each hop) reads as chained hops with one
// ground frame before each jump, a turn as hard as the move allows and no lag, gaining speed;
// a walker reads as no jumps; a step of hundreds of units and a dead player cut the track; the
// air's most turn at 300 units/s and 77 commands a second is about 440 degrees a second. On the
// nav course the pilot's hop mode, with practised hands, runs the long straight faster than its
// walk.
//
// ge:summary Tests: the hop reader on an ideal hopper and a walker, cuts, max_turn, the pilot hop
// ge:keywords test hops bunny-hop read_hops chain max_turn turn-share lag cut pilot-hop
// ge:skill shooter bots testing
#include <string>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/hops.hpp"
#include "shooter/pilot.hpp"
#include "shooter/pmove.hpp"
#include "shooters_edge/bots/pilot_run.hpp"

namespace sh = ge::shooter;
namespace pr = shooters_edge::bots;

namespace {

sh::Brush box(int contents, sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.contents = contents;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

struct Floor {
  sh::HullData point, clip;
  sh::MoveWorld world;
  Floor() {
    const std::vector<sh::Brush> brushes = {
        box(sh::kContentsSolid, {-8192, -8192, -64}, {8192, 8192, 0})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.count = 1;
  }
};

const Floor& floor_world() {
  static const Floor f;
  return f;
}

constexpr double kRadToDeg = 57.29577951308232;

// `seconds` of commands from standing at the origin running along +x at 320: `hop` jumps on the
// command it lands, the side key alone, the view on the velocity, the side switched each jump;
// otherwise it walks forward.
std::vector<sh::TrackPoint> drive(bool hop, double seconds) {
  const Floor& f = floor_world();
  sh::MoveState s;
  s.origin = {0, 0, 24.03125f};
  s.velocity = {320, 0, 0};
  s.ground = 0;
  s.ground_normal = {0, 0, 1};
  const sh::MoveSettings set;
  std::vector<sh::TrackPoint> track;
  int side = 1;
  const int n = static_cast<int>(seconds * 1000 / pr::kPilotMsec);
  for (int i = 0; i < n; i++) {
    sh::MoveCmd c;
    c.msec = pr::kPilotMsec;
    const float heading =
        static_cast<float>(ge::core::libm::atan2(s.velocity.y, s.velocity.x) * kRadToDeg);
    c.angles = {0, heading, 0};
    if (hop) {
      if (s.ground >= 0 && !s.jump_held) {
        c.buttons = sh::kButtonJump;
        side = -side;
      }
      c.side = static_cast<int16_t>(side * 400);
    } else {
      c.forward = 400;
    }
    sh::player_move(s, c, set, f.world);
    sh::TrackPoint p;
    p.t = (i + 1) * pr::kPilotMsec / 1000.0;
    p.origin = s.origin;
    p.yaw = heading;
    track.push_back(p);
  }
  return track;
}

}  // namespace

TEST(the_air_turns_at_most_atan_30_over_v_a_command) {
  const float t = sh::max_turn(300, 77);
  CHECK_MSG(t > 438 && t < 441, "max_turn(300, 77) is " + std::to_string(t) + ", not about 440");
  CHECK(sh::max_turn(600, 77) < t / 1.9f);
}

TEST(an_ideal_hopper_reads_as_chained_hops_turning_hard_with_no_lag) {
  const std::vector<sh::TrackPoint> track = drive(true, 6);
  sh::HopReading r;
  sh::read_hops(track, floor_world().world, sh::HopSettings{}, r);
  const sh::HopSummary& m = r.summary;
  CHECK_MSG(m.jumps >= 8, "only " + std::to_string(m.jumps) + " jumps in 6 s of hopping");
  CHECK_MSG(m.chained >= m.jumps - 1, std::to_string(m.chained) + " of " + std::to_string(m.jumps) +
                                          " jumps read as chained");
  CHECK(m.ground_before[1] >= m.jumps - 1);
  CHECK(m.longest_chain >= m.jumps - 1);
  CHECK_MSG(m.turn_share_p50 > 0.8f,
            "the turn share is " + std::to_string(m.turn_share_p50) + ", not near the most");
  // The view is on the velocity, so the lag is the frames' differencing alone.
  CHECK_MSG(m.lag_p50 < 6, "the lag reads " + std::to_string(m.lag_p50) + " degrees");
  CHECK_MSG(m.speed_top > 450, "the hops reached only " + std::to_string(m.speed_top) + " u/s");
  CHECK(m.gain_per_s > 30);
  CHECK(m.air_share > 0.8f);
}

TEST(a_walker_reads_as_no_jumps_on_the_ground) {
  const std::vector<sh::TrackPoint> track = drive(false, 3);
  sh::HopReading r;
  sh::read_hops(track, floor_world().world, sh::HopSettings{}, r);
  CHECK(r.summary.jumps == 0);
  CHECK(r.summary.air_share < 0.05f);
  CHECK(r.summary.speed_top < 330);
}

TEST(a_teleport_and_a_death_cut_the_track) {
  std::vector<sh::TrackPoint> track = drive(false, 1);
  const size_t mid = track.size() / 2;
  for (size_t i = mid; i < track.size(); i++) track[i].origin.x += 900;  // a teleport
  track[mid / 2].dead = true;
  sh::HopReading r;
  sh::read_hops(track, floor_world().world, sh::HopSettings{}, r);
  CHECK(r.frames.size() == track.size());
  CHECK(r.frames[mid / 2].phase == sh::HopPhase::cut);
  // Neither side of the jump is differenced across it.
  CHECK(r.frames[mid - 1].phase == sh::HopPhase::cut || r.frames[mid].phase == sh::HopPhase::cut);
  for (const sh::HopFrame& fr : r.frames)
    CHECK_MSG(fr.speed < 400,
              "a frame was differenced across the cut: " + std::to_string(fr.speed) + " u/s");
}

TEST(the_pilot_hops_the_long_straight_faster_than_it_walks) {
  const std::unique_ptr<pr::Course> c = pr::make_course();
  const pr::CourseRoute r = pr::course_long_straight(*c);
  sh::PilotSettings walk = pr::course_strafe_settings();
  walk.strafe = false;
  sh::PilotSettings hop = walk;
  hop.hop = true;
  const pr::RunResult a = pr::run_course(*c, r.from, r.to, sh::PilotHands::none, 1, walk);
  const pr::RunResult b = pr::run_course(*c, r.from, r.to, sh::PilotHands::practised, 1, hop);
  CHECK(a.arrived && b.arrived);
  CHECK_MSG(b.stats.hops >= 1 && b.stats.hop_jumps >= 2,
            "the long straight was not hopped: " + std::to_string(b.stats.hop_jumps) + " jumps");
  CHECK_MSG(b.top_speed > a.top_speed + 20, "hopping reached " + std::to_string(b.top_speed) +
                                                " u/s against walking's " +
                                                std::to_string(a.top_speed));
}
