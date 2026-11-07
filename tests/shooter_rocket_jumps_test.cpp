// ROCKET JUMPS READ OFF A TRACK (plugins/shooter/rocket_jumps.hpp): the pilot flies a running
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
#include "shooter/nav.hpp"
#include "shooter/rocket_jumps.hpp"
#include "shooters_edge/bots/pilot_run.hpp"

namespace sh = ge::shooter;
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
  sh::NavGraph g;
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
    sh::build_nav(world, {}, sh::MoveSettings{}, sh::NavSettings{}, g);
    ge::ai::Search search;
    sh::nav_reach(g, sh::nav_nearest(g, from), search);
    std::vector<uint8_t> main(g.nodes.size());
    for (size_t n = 0; n < main.size(); n++) main[n] = search.reached(static_cast<int32_t>(n));
    sh::nav_bridge_pockets(world, sh::MoveSettings{}, g, main);
  }
};

// The pilot's run as a recording would hold it: a point a command, and the rockets it fired.
std::vector<sh::RocketJump> flown(const Room& room, sh::Vec3f from, sh::Vec3f to,
                                  sh::PilotHands hands, std::vector<sh::RocketShot>* shots_out) {
  sh::PilotSettings set;
  set.rockets = true;
  const pr::RunResult r = pr::run_route(room.world, room.g, from, to, hands, 1, set);
  std::vector<sh::TrackPoint> track;
  for (size_t i = 0; i < r.trail.size(); i++)
    track.push_back({static_cast<double>(i + 1) * (pr::kPilotMsec * 0.001), r.trail[i],
                     static_cast<float>(r.yaw[i]), r.pitch[i], false});
  sh::HopReading hops;
  sh::read_hops(track, room.world, sh::HopSettings{}, hops);
  std::vector<sh::RocketJump> out;
  sh::read_rocket_jumps(track, hops, r.rockets, sh::RocketJumpReadSettings{}, out);
  if (shots_out) *shots_out = r.rockets;
  return out;
}

std::string about(const std::vector<sh::RocketJump>& js) {
  std::string s = std::to_string(js.size()) + " jumps";
  for (const sh::RocketJump& j : js)
    s += std::string("; ") + sh::rocket_jump_kind_name(j.kind) + (j.standing ? " standing" : "") +
         " pitch " + std::to_string(j.pitch) + " height " + std::to_string(j.height) + " keys " +
         sh::sequence_name(j.keys_run) + " shot " + sh::keys_name(j.keys_shot) + " air " +
         sh::sequence_name(j.keys_air);
  return s;
}

}  // namespace

TEST(a_running_flip_reads_as_a_flip_run_at_and_landed_on_the_block) {
  const Room room({box({-384, -128, -64}, {384, 128, 0}), box({96, -128, 0}, {384, 128, 160})},
                  {-384, -128, -64}, {384, 128, 512}, {-256, 0, 24});
  for (const sh::PilotHands hands : {sh::PilotHands::none, sh::PilotHands::human}) {
    const std::vector<sh::RocketJump> js =
        flown(room, {-256, 0, 24}, {320, 0, 184}, hands, nullptr);
    CHECK_MSG(js.size() == 1, "one flip flown, read as " + about(js));
    if (js.size() != 1) continue;
    const sh::RocketJump& j = js[0];
    CHECK_MSG(j.kind == sh::RocketJumpKind::flip && !j.standing, "read as " + about(js));
    CHECK_MSG(j.pitch > 30 && j.pitch < 80,
              "read as " + about(js) + ": the flip fires 35..75 down");
    CHECK_MSG(j.land >= 0 && j.height > 150 && j.height < 170,
              "the flip landed " + std::to_string(j.height) + " over its start, not on the block");
    CHECK_MSG(j.keys_run.has(sh::kKeyLeft | sh::kKeyRight | sh::kKeyBack) &&
                  (j.keys_shot & sh::kKeyBack) != 0,
              "the turn into the flip read no strafe and no back key at the shot: " + about(js));
  }
}

TEST(a_standing_flip_reads_as_standing) {
  const Room room({box({0, -64, -64}, {384, 64, 0}), box({64, -64, 0}, {384, 64, 128})},
                  {0, -64, -64}, {384, 64, 512}, {32, 0, 24});
  const std::vector<sh::RocketJump> js =
      flown(room, {32, 0, 24}, {320, 0, 152}, sh::PilotHands::none, nullptr);
  CHECK_MSG(js.size() == 1, "one standing flip flown, read as " + about(js));
  if (js.size() == 1)
    CHECK_MSG(js[0].kind == sh::RocketJumpKind::flip && js[0].standing, "read as " + about(js));
}

TEST(a_rocket_gone_far_from_the_player_is_no_jump) {
  const Room room({box({-384, -128, -64}, {384, 128, 0}), box({96, -128, 0}, {384, 128, 160})},
                  {-384, -128, -64}, {384, 128, 512}, {-256, 0, 24});
  std::vector<sh::RocketShot> shots;
  flown(room, {-256, 0, 24}, {320, 0, 184}, sh::PilotHands::none, &shots);
  CHECK(!shots.empty());
  // The same track with its rocket moved 400 units off: no jump.
  sh::PilotSettings set;
  set.rockets = true;
  const pr::RunResult r =
      pr::run_route(room.world, room.g, {-256, 0, 24}, {320, 0, 184}, sh::PilotHands::none, 1, set);
  std::vector<sh::TrackPoint> track;
  for (size_t i = 0; i < r.trail.size(); i++)
    track.push_back({static_cast<double>(i + 1) * (pr::kPilotMsec * 0.001), r.trail[i],
                     static_cast<float>(r.yaw[i]), r.pitch[i], false});
  sh::HopReading hops;
  sh::read_hops(track, room.world, sh::HopSettings{}, hops);
  std::vector<sh::RocketShot> far = r.rockets;
  for (sh::RocketShot& s : far) s.to = s.to + sh::Vec3f{0, 400, 0};
  std::vector<sh::RocketJump> js;
  sh::read_rocket_jumps(track, hops, far, sh::RocketJumpReadSettings{}, js);
  CHECK_MSG(js.empty(), "a rocket 400 units off read as " + about(js));
}

TEST(a_wish_reads_as_the_keys_of_its_side_of_the_view) {
  // The view along +x: a wish along +x is forward, along -y right, both together both.
  CHECK(sh::keys_of_wish({1000, 0, 0}, 0, 100) == sh::kKeyForward);
  CHECK(sh::keys_of_wish({-1000, 0, 0}, 0, 100) == sh::kKeyBack);
  CHECK(sh::keys_of_wish({0, -1000, 0}, 0, 100) == sh::kKeyRight);
  CHECK(sh::keys_of_wish({0, 1000, 0}, 0, 100) == sh::kKeyLeft);
  CHECK(sh::keys_of_wish({700, -700, 0}, 0, 100) == (sh::kKeyForward | sh::kKeyRight));
  CHECK(sh::keys_of_wish({50, 0, 0}, 0, 100) == 0);
  // The view turned round: the same wish is the back key.
  CHECK(sh::keys_of_wish({1000, 0, 0}, 180, 100) == sh::kKeyBack);
  CHECK(sh::keys_name(sh::kKeyBack | sh::kKeyLeft) == "B+L");
}
