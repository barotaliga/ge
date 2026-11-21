// ROCKET JUMPS READ OFF A TRACK (plugins/shooterbots/rocket_jumps.hpp): the pilot flies a running
// flip onto a block and a standing one in a shallow room (as the pilot test does), its track and
// rockets kept; the reader finds each one jump, a flip, running or standing as flown, at the
// pitch fired, landed on the block's top, and the keys of a turn strafed into it; a rocket gone
// far from the player is no jump; and a wish reads as the keys of its side of the view.
//
// ge:summary Tests: rocket jumps read off the pilot's own flights: kind, standing, pitch, keys
// ge:keywords test rocket-jump flip read keys wish track demo
// ge:skill shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooterbots/nav.hpp"
#include "shooterbots/rocket_jumps.hpp"
#include "shooters_edge/bots/pilot_run.hpp"

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;
namespace pr = shooters_edge::bots;

namespace {

sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

// A walled room of boxes with its graph, the pockets bridged with rocket jumps from `from`.
struct Room {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sb::NavGraph g;
  Room(std::vector<sh::Brush> brushes, sh::Vec3f lo, sh::Vec3f hi, sh::Vec3f from) {
    brushes.push_back(box({lo.x - 32, lo.y - 32, lo.z}, {lo.x, hi.y + 32, hi.z}));
    brushes.push_back(box({hi.x, lo.y - 32, lo.z}, {hi.x + 32, hi.y + 32, hi.z}));
    brushes.push_back(box({lo.x - 32, lo.y - 32, lo.z}, {hi.x + 32, lo.y, hi.z}));
    brushes.push_back(box({lo.x - 32, hi.y, lo.z}, {hi.x + 32, hi.y + 32, hi.z}));
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {lo.x - 32, lo.y - 32, lo.z};
    world.solids[0].maxs = {hi.x + 32, hi.y + 32, hi.z};
    world.count = 1;
    sb::build_nav(world, {}, sh::MoveSettings{}, sb::NavSettings{}, g);
    ge::ai::Search search;
    sb::nav_reach(g, sb::nav_nearest(g, from), search);
    std::vector<uint8_t> main(g.nodes.size());
    for (size_t n = 0; n < main.size(); n++) main[n] = search.reached(static_cast<int32_t>(n));
    sb::nav_bridge_pockets(world, sh::MoveSettings{}, g, main);
  }
};

// The pilot's run as a recording would hold it: a point a command, and the rockets it fired.
std::vector<sb::RocketJump> flown(const Room& room, sh::Vec3f from, sh::Vec3f to,
                                  sb::PilotHands hands, std::vector<sb::RocketShot>* shots_out) {
  sb::PilotSettings set;
  set.jumper.rockets = true;
  const pr::RunResult r = pr::run_route(room.world, room.g, from, to, hands, 1, set);
  std::vector<sb::TrackPoint> track;
  for (size_t i = 0; i < r.trail.size(); i++)
    track.push_back({static_cast<double>(i + 1) * (pr::kPilotMsec * 0.001), r.trail[i],
                     static_cast<float>(r.yaw[i]), r.pitch[i], false});
  sb::HopReading hops;
  sb::read_hops(track, room.world, sb::HopSettings{}, hops);
  std::vector<sb::RocketJump> out;
  sb::read_rocket_jumps(track, hops, r.rockets, sb::RocketJumpReadSettings{}, out);
  if (shots_out) *shots_out = r.rockets;
  return out;
}

std::string about(const std::vector<sb::RocketJump>& js) {
  std::string s = std::to_string(js.size()) + " jumps";
  for (const sb::RocketJump& j : js)
    s += std::string("; ") + sb::rocket_jump_kind_name(j.kind) + (j.standing ? " standing" : "") +
         " pitch " + std::to_string(j.pitch) + " height " + std::to_string(j.height) + " keys " +
         sb::sequence_name(j.keys_run) + " shot " + sb::keys_name(j.keys_shot) + " air " +
         sb::sequence_name(j.keys_air);
  return s;
}

}  // namespace

TEST(a_running_flip_reads_as_a_flip_run_at_and_landed_on_the_block) {
  const Room room({box({-384, -128, -64}, {384, 128, 0}), box({96, -128, 0}, {384, 128, 160})},
                  {-384, -128, -64}, {384, 128, 512}, {-256, 0, 24});
  for (const sb::PilotHands hands : {sb::PilotHands::none, sb::PilotHands::human}) {
    const std::vector<sb::RocketJump> js =
        flown(room, {-256, 0, 24}, {320, 0, 184}, hands, nullptr);
    CHECK_MSG(js.size() == 1, "one flip flown, read as " + about(js));
    if (js.size() != 1) continue;
    const sb::RocketJump& j = js[0];
    CHECK_MSG(j.kind == sb::RocketJumpKind::flip && !j.standing, "read as " + about(js));
    CHECK_MSG(j.pitch > 30 && j.pitch < 80,
              "read as " + about(js) + ": the flip fires 35..75 down");
    CHECK_MSG(j.land >= 0 && j.height > 150 && j.height < 170,
              "the flip landed " + std::to_string(j.height) + " over its start, not on the block");
    CHECK_MSG(j.keys_run.has(sb::kKeyLeft | sb::kKeyRight | sb::kKeyBack) &&
                  (j.keys_shot & sb::kKeyBack) != 0,
              "the turn into the flip read no strafe and no back key at the shot: " + about(js));
  }
}

TEST(a_standing_flip_reads_as_standing) {
  const Room room({box({0, -64, -64}, {384, 64, 0}), box({64, -64, 0}, {384, 64, 128})},
                  {0, -64, -64}, {384, 64, 512}, {32, 0, 24});
  const std::vector<sb::RocketJump> js =
      flown(room, {32, 0, 24}, {320, 0, 152}, sb::PilotHands::none, nullptr);
  CHECK_MSG(js.size() == 1, "one standing flip flown, read as " + about(js));
  if (js.size() == 1)
    CHECK_MSG(js[0].kind == sb::RocketJumpKind::flip && js[0].standing, "read as " + about(js));
}

TEST(a_rocket_gone_far_from_the_player_is_no_jump) {
  const Room room({box({-384, -128, -64}, {384, 128, 0}), box({96, -128, 0}, {384, 128, 160})},
                  {-384, -128, -64}, {384, 128, 512}, {-256, 0, 24});
  std::vector<sb::RocketShot> shots;
  flown(room, {-256, 0, 24}, {320, 0, 184}, sb::PilotHands::none, &shots);
  CHECK(!shots.empty());
  // The same track with its rocket moved 400 units off: no jump.
  sb::PilotSettings set;
  set.jumper.rockets = true;
  const pr::RunResult r =
      pr::run_route(room.world, room.g, {-256, 0, 24}, {320, 0, 184}, sb::PilotHands::none, 1, set);
  std::vector<sb::TrackPoint> track;
  for (size_t i = 0; i < r.trail.size(); i++)
    track.push_back({static_cast<double>(i + 1) * (pr::kPilotMsec * 0.001), r.trail[i],
                     static_cast<float>(r.yaw[i]), r.pitch[i], false});
  sb::HopReading hops;
  sb::read_hops(track, room.world, sb::HopSettings{}, hops);
  std::vector<sb::RocketShot> far = r.rockets;
  for (sb::RocketShot& s : far) s.to = s.to + sh::Vec3f{0, 400, 0};
  std::vector<sb::RocketJump> js;
  sb::read_rocket_jumps(track, hops, far, sb::RocketJumpReadSettings{}, js);
  CHECK_MSG(js.empty(), "a rocket 400 units off read as " + about(js));
}

TEST(a_wish_reads_as_the_keys_of_its_side_of_the_view) {
  // The view along +x: a wish along +x is forward, along -y right, both together both.
  CHECK(sb::keys_of_wish({1000, 0, 0}, 0, 100) == sb::kKeyForward);
  CHECK(sb::keys_of_wish({-1000, 0, 0}, 0, 100) == sb::kKeyBack);
  CHECK(sb::keys_of_wish({0, -1000, 0}, 0, 100) == sb::kKeyRight);
  CHECK(sb::keys_of_wish({0, 1000, 0}, 0, 100) == sb::kKeyLeft);
  CHECK(sb::keys_of_wish({700, -700, 0}, 0, 100) == (sb::kKeyForward | sb::kKeyRight));
  CHECK(sb::keys_of_wish({50, 0, 0}, 0, 100) == 0);
  // The view turned round: the same wish is the back key.
  CHECK(sb::keys_of_wish({1000, 0, 0}, 180, 100) == sb::kKeyBack);
  CHECK(sb::keys_name(sb::kKeyBack | sb::kKeyLeft) == "B+L");
}
