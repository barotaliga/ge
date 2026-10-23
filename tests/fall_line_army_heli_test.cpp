// FALL LINE'S ARMY HELICOPTERS, HELD (games/fall_line/sim/army_heli.hpp, army_land.hpp): the
// landing zone (flat clear ground ahead of him, side on to him; a fast rope's spot where the woods
// close round him), and on the flat drag strip the call, the way in over the map's edge, the pylon
// turn round him, a section set down near him that kneels until the machine has lifted and then
// goes after him, the fast rope in the woods, home once the hunt is over, and a trace that records
// the machines' decisions and changes nothing; a scripted hunt on seed 38 held bit for bit by
// tests/golden/fall_line_army_heli.txt.
//
// The hunted man stands still; the stars are pinned (his heat held each step). A deliberate
// change to the machines' rules or numbers moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_army_heli_test`, and put the `army-flight`
// lab's table before and after in the PR.
//
// ge:summary Test: the army's helicopters: landing zone, rope, pylon turn, drop, home; golden
// ge:keywords test army helicopter army-heli land_spot fast-rope pylon-turn orbit drop kneel
// ge:keywords home why trace station golden digest
// ge:skill sim testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "ai/why.hpp"
#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_heli.hpp"
#include "fall_line/sim/army_heli_rules.hpp"
#include "fall_line/sim/army_land.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "golden.hpp"
#include "skier/skier.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace ah = fall_line::sim::army_heli;
namespace lm = ge::core::libm;
using ge::core::angle_diff;
double dist(double a, double b) { return ge::core::hypot(a, b); }
constexpr const char* kGolden = "tests/golden/fall_line_army_heli.txt";
// Where he stands on the strip, m (its middle).
constexpr double kMid = sk::kStripSize / 2;

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

std::string hex(uint64_t u) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u));
  return b;
}

// The flat strip; with `woods`, spruces every 12 m in a ring 40 to 320 m round its middle, too
// close together for a machine to set down between them and far enough apart for a man's drop.
std::shared_ptr<const mg::Level> strip(bool woods) {
  mg::Level level = sk::flat_strip(0, 1);
  if (woods) {
    for (int32_t j = 0; j <= 53; j++) {
      const double z = kMid - 320 + 12 * j;
      for (int32_t i = 0; i <= 53; i++) {
        const double x = kMid - 320 + 12 * i;
        const double r = dist(x - kMid, z - kMid);
        if (r < 40 || r > 320) continue;
        level.trees.push_back(mg::TreeDef{x, z, 2000, 15, 0.25, 60, 3, mg::TreeKind::spruce, -1});
      }
    }
  }
  return std::make_shared<const mg::Level>(std::move(level));
}

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(38, 10));
  return level;
}

// A HUNT ON THE STRIP: he stands still at its middle at `stars` for `hunted` s, then the hunt is
// called off and the run goes on `after` s. What the machines and their men did.
struct Hunt {
  fs::Spot q;
  std::vector<fs::GameEvent> events;
  double arrived = -1;                    // s, the first machine in over the edge
  double in_from = 0;                     // m, how far from the map's middle it came in
  double circle_lo = 1e9, circle_hi = 0;  // m, its distance from him over the hunt's last minute
  double up_lo = 1e9, up_hi = -1e9;       // m, its height over him over the same
  double under_floor = 0;                 // m, the most it was ever under its floor while it flew
  double landed_at = -1;                  // m, the landing's spot from him
  bool set_on_snow = false;               // on the snow when set
  bool side_on = false;                   // set down side on to him
  bool roped = false;                     // a man seen on the rope
  bool knelt_till_lift = true;            // no man left `out` before his machine lifted
  int32_t chasing = 0;  // the men they put down chasing or holding at the hunt's end
  int32_t dropped = 0;
  int32_t left_after = 0;  // machines out at the end
  uint64_t hash = 0;
  int32_t stations = 0;  // station decisions recorded
  int32_t station_agent = -1;
};

Hunt hunt_on(const std::shared_ptr<const mg::Level>& level, int32_t stars, double hunted,
             double after, ge::ai::Why* why = nullptr) {
  fs::GameState st = fs::create_game_on(level, 1);
  st.why = why;
  Hunt h;
  st.racers[0].skier = sk::stand_skier(st.world(), kMid, kMid, 0, 0);
  fs::set_wanted(st, stars);
  h.q = fs::quarry_of(st);
  const fs::Controls none{};
  const auto steps = static_cast<int64_t>(std::llround((hunted + after) * fs::kHz));
  bool lifted = false;
  for (int64_t i = 0; i < steps; i++) {
    const bool on = st.t < hunted;
    fs::WantedState& w = st.wanted;
    if (on && w.ended == fs::WantedEnd::running && w.stars > 0) w.heat = stars;
    if (!on && w.ended == fs::WantedEnd::running && w.stars > 0) {
      h.chasing = 0;
      for (const fs::Soldier& s : st.army.soldiers) {
        const bool going = s.mode == fs::SoldierMode::chase || s.mode == fs::SoldierMode::hold;
        h.chasing += s.from == fs::SoldierFrom::heli && going ? 1 : 0;
      }
      fs::set_wanted(st, 0);
    }
    st.racers[0].skier = sk::stand_skier(st.world(), kMid, kMid, 0, 0);
    if (why) why->clear();
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    if (why) {
      for (const ge::ai::Decision& d : why->decisions()) {
        if (std::string(d.name) != "station") continue;
        h.stations++;
        h.station_agent = d.agent;
      }
    }
    for (const fs::GameEvent& e : st.events) {
      if (e.kind != fs::EventKind::army_heli) continue;
      h.events.push_back(e);
      const auto c = static_cast<fs::ArmyHeliChange>(e.gate);
      if (c == fs::ArmyHeliChange::arrived && h.arrived < 0) {
        h.arrived = st.t;
        const fs::ArmyHeli& m = st.army.helis.front();
        h.in_from = dist(m.x - kMid, m.z - kMid);
      }
      if (c == fs::ArmyHeliChange::landed && h.landed_at < 0) {
        const fs::ArmyHeli& m = st.army.helis.front();
        h.landed_at = dist(m.task.x - h.q.x, m.task.z - h.q.z);
        h.set_on_snow =
            m.mode == fs::ArmyHeliMode::set && std::fabs(m.y - st.level->ground_at(m.x, m.z)) < 0.5;
        const double to_him = lm::atan2(h.q.x - m.task.x, h.q.z - m.task.z);
        h.side_on =
            std::fabs(std::fabs(angle_diff(m.task.heading, to_him)) - ge::core::kPi / 2) < 0.01;
      }
    }
    for (const fs::ArmyHeli& m : st.army.helis) lifted = lifted || m.mode == fs::ArmyHeliMode::rise;
    for (const fs::Soldier& s : st.army.soldiers) {
      h.roped = h.roped || s.mode == fs::SoldierMode::rope;
      if (!lifted && s.mode == fs::SoldierMode::chase && s.from == fs::SoldierFrom::heli) {
        h.knelt_till_lift = false;
      }
    }
    for (const fs::ArmyHeli& m : st.army.helis) {
      if (m.mode == fs::ArmyHeliMode::hunt || m.mode == fs::ArmyHeliMode::leave) {
        const double floor = fs::army_floor(st, m.x, m.z, ah::kFloorM);
        h.under_floor = std::fmax(h.under_floor, floor - m.y);
      }
      if (on && st.t > hunted - 60 && m.mode == fs::ArmyHeliMode::hunt && m.id == 0) {
        const double d = dist(m.x - h.q.x, m.z - h.q.z);
        h.circle_lo = std::fmin(h.circle_lo, d);
        h.circle_hi = std::fmax(h.circle_hi, d);
        h.up_lo = std::fmin(h.up_lo, m.y - h.q.y);
        h.up_hi = std::fmax(h.up_hi, m.y - h.q.y);
      }
    }
  }
  h.dropped = st.army.dropped;
  h.left_after = static_cast<int32_t>(st.army.helis.size());
  h.hash = fs::state_hash(st, 0xcbf29ce484222325ull);
  return h;
}

int count(const std::vector<fs::GameEvent>& events, fs::ArmyHeliChange c) {
  int n = 0;
  for (const auto& e : events) n += static_cast<fs::ArmyHeliChange>(e.gate) == c ? 1 : 0;
  return n;
}

}  // namespace

TEST(a_landing_is_flat_clear_ground_ahead_of_him_side_on_and_a_rope_where_the_woods_close_round) {
  const fs::GameState open = fs::create_game_on(strip(false), 1);
  // Him running down +z: the place is ahead of him, near the preferred distance, side on to him.
  const auto a = fs::land_spot(open, kMid, 2000, kMid, 0, 10, {}, fs::drop_ring());
  CHECK(a.has_value());
  if (a) {
    CHECK(!a->rope);
    CHECK(a->z > kMid);
    CHECK(std::fabs(dist(a->x - kMid, a->z - kMid) - ah::kPreferM) < ah::kGridM * 1.5);
    const double to_him = lm::atan2(kMid - a->x, kMid - a->z);
    CHECK(std::fabs(std::fabs(angle_diff(a->heading, to_him)) - ge::core::kPi / 2) < 1e-9);
  }
  // A circle it must keep out of moves it.
  const fs::LandAvoid keep{a ? a->x : 0, a ? a->z : 0, ah::kAvoidM};
  const auto b = fs::land_spot(open, kMid, 2000, kMid, 0, 10, std::span(&keep, 1), fs::drop_ring());
  CHECK(b.has_value());
  if (a && b) CHECK(dist(b->x - a->x, b->z - a->z) >= ah::kAvoidM);
  // In the woods nothing is clear enough to set down: a fast rope, its hover within reach.
  const fs::GameState woods = fs::create_game_on(strip(true), 1);
  const auto r = fs::land_spot(woods, kMid, 2000, kMid, 0, 0, {}, fs::drop_ring());
  CHECK(r.has_value());
  if (r) {
    CHECK(r->rope);
    CHECK(fs::rope_hover(woods, r->x, r->z, r->y) - r->y <= ah::kRopeReachM);
  }
}

TEST(called_at_one_star_it_comes_in_over_the_edge_and_flies_a_pylon_turn_round_him) {
  const Hunt h = hunt_on(strip(false), 1, 150, 0);
  CHECK(count(h.events, fs::ArmyHeliChange::called) == fs::roster_of(1).helicopters);
  CHECK(h.arrived >= ah::kRespondS);
  CHECK(h.arrived < ah::kRespondS + 0.1);
  CHECK(h.in_from > sk::kStripSize / 2 - ah::kInsetM - 1);
  // Round him at its circle's radius and height, never under its floor; nobody set down at one
  // star (the roster sends no soldiers).
  CHECK(h.circle_lo > ah::kOrbitM - 40);
  CHECK(h.circle_hi < ah::kOrbitM + 40);
  CHECK(h.up_lo > ah::kOrbitUpM - 15);
  CHECK(h.up_hi < ah::kOrbitUpM + 15);
  CHECK(h.under_floor < 1e-9);
  CHECK(count(h.events, fs::ArmyHeliChange::landed) == 0);
}

TEST(at_two_stars_it_sets_a_section_down_that_kneels_until_it_lifts_and_then_goes_after_him) {
  const Hunt h = hunt_on(strip(false), 2, 150, 0);
  CHECK(count(h.events, fs::ArmyHeliChange::landed) == 1);
  CHECK(count(h.events, fs::ArmyHeliChange::lifted) == 1);
  CHECK(h.landed_at >= ah::kNearM);
  CHECK(h.landed_at <= ah::kFarM);
  CHECK(h.set_on_snow);
  CHECK(h.side_on);
  CHECK(h.dropped == fs::roster_of(2).soldiers);
  CHECK(h.knelt_till_lift);
  CHECK(!h.roped);
}

TEST(with_nowhere_to_set_down_the_section_comes_down_a_fast_rope) {
  const Hunt h = hunt_on(strip(true), 2, 120, 0);
  CHECK(count(h.events, fs::ArmyHeliChange::landed) == 1);
  CHECK(h.roped);
  CHECK(h.dropped == fs::roster_of(2).soldiers);
}

TEST(the_hunt_over_they_fly_home_over_the_edge_and_are_gone) {
  const Hunt h = hunt_on(strip(false), 2, 120, 120);
  CHECK(count(h.events, fs::ArmyHeliChange::leaving) >= 1);
  CHECK(count(h.events, fs::ArmyHeliChange::gone) >= 1);
  CHECK(h.left_after == 0);
  CHECK(h.chasing == fs::roster_of(2).soldiers);
}

TEST(a_trace_records_each_machines_station_and_decides_nothing) {
  ge::ai::Why why(ah::kAgentBase);
  const Hunt traced = hunt_on(strip(false), 1, 60, 0, &why);
  const Hunt plain = hunt_on(strip(false), 1, 60, 0);
  CHECK(traced.hash == plain.hash);
  CHECK(traced.stations > 0);
  CHECK(traced.station_agent == ah::kAgentBase);
}

TEST(a_scripted_hunt_on_seed_38_keeps_its_golden) {
  // He stands on open snow up the mountain from the village at three stars for 150 s, the army's
  // every arm sent; then the hunt is called off for 150 s. Each half minute: the digest, the
  // machines out, the men they put down out, and the landings so far.
  fs::GameState st = fs::create_free_ride(seed38(), 0);
  const double fx = lm::sin(st.army.pad_heading);
  const double fz = lm::cos(st.army.pad_heading);
  const double x = st.army.pad_x + fx * 400;
  const double z = st.army.pad_z + fz * 400;
  st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
  fs::set_wanted(st, 3);
  const fs::Controls none{};
  std::vector<std::string> got;
  int32_t landed = 0;
  for (int64_t i = 0; i < std::llround(300 * fs::kHz); i++) {
    fs::WantedState& w = st.wanted;
    if (st.t < 150 && w.ended == fs::WantedEnd::running && w.stars > 0) w.heat = 3;
    if (st.t >= 150 && w.ended == fs::WantedEnd::running && w.stars > 0) fs::set_wanted(st, 0);
    st.racers[0].skier = sk::stand_skier(st.world(), x, z, 0, 0);
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (const fs::GameEvent& e : st.events) {
      landed += e.kind == fs::EventKind::army_heli &&
                        static_cast<fs::ArmyHeliChange>(e.gate) == fs::ArmyHeliChange::landed
                    ? 1
                    : 0;
    }
    if (st.steps % static_cast<int64_t>(30 * fs::kHz) == 0) {
      int32_t men = 0;
      for (const fs::Soldier& s : st.army.soldiers) men += s.from == fs::SoldierFrom::heli ? 1 : 0;
      got.push_back(hex(fs::state_hash(st, 0xcbf29ce484222325ull)) + " " +
                    std::to_string(st.army.helis.size()) + " " + std::to_string(men) + " " +
                    std::to_string(landed));
    }
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# each half minute: state digest, helicopters out, men they put down on the map, "
           "landings so far (fall_line_army_heli_test, the seed 38 hunt at three stars for 150 s, "
           "then called off for 150 s)\n";
    for (const std::string& row : got) out << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  CHECK(want.size() == got.size());
  for (size_t r = 0; r < got.size() && r < want.size(); r++) {
    std::string line;
    for (const auto& w : want[r]) line += (line.empty() ? "" : " ") + w;
    CHECK_MSG(line == got[r], "the seed 38 hunt moved at row " + std::to_string(r) + ":\n  got  " +
                                  got[r] + "\n  want " + line + "\nIf the move is meant, rewrite " +
                                  kGolden +
                                  " with GE_GOLDEN_WRITE=1 and put the army-flight lab's table "
                                  "before and after in the PR.");
  }
}
