// FALL LINE'S ARMY SNOWMOBILES, HELD (games/fall_line/sim/army_sled.hpp, army_drive.hpp): the way
// read off the snow (round a wall of trunks, across a face steeper than a laden machine climbs,
// the wind tunnels a wall), and on seed 38 the call, the drive from the village, the short halt
// side-on to him outside the stand-off, the crews in the pursuers, home once the hunt is over, the
// crew off to take him in where the army does not shoot to kill, and none sent on a race's map; a
// scripted hunt there held bit for bit by tests/golden/fall_line_army_sled.txt.
//
// The hunted man stands still on open snow up the mountain from the village; the stars are pinned
// (his heat held each step) and a helicopter hovers over him so the hunt keeps sight of him. A
// deliberate change to the machines' rules or numbers moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_army_sled_test`, and put the `army-sleds` lab's
// table before and after in the PR.
//
// ge:summary Test: the army's snowmobiles: the way read, the call, halt, home, arrest; golden
// ge:keywords test army snowmobile sled choose_way tunnel halt stand-off home dismount arrest
// ge:keywords golden digest
// ge:skill sim testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_drive.hpp"
#include "fall_line/sim/army_sled.hpp"
#include "fall_line/sim/army_sled_rules.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "golden.hpp"
#include "skier/skier.hpp"
#include "skier/synthetic.hpp"
#include "skier/trees.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace as = fall_line::sim::army_sled;
namespace lm = ge::core::libm;
using ge::core::angle_diff;
double dist(double a, double b) { return ge::core::hypot(a, b); }
constexpr const char* kGolden = "tests/golden/fall_line_army_sled.txt";

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

// What the machines read: the army's own numbers.
fs::DriveRead read() {
  return fs::DriveRead{as::kLookM,       as::kStepM,    as::kFan,     as::kSpread, as::kRise,
                       as::kDrop,        as::kTunnelM,  as::kTrunkM,  as::kEdgeM,  as::kTurnCost,
                       as::kBlockedCost, as::kTreeCost, as::kKeepCost};
}

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(38, 10));
  return level;
}

// Open snow some 350 m up the mountain from the village's machines: the first spot along the way
// they face with no trunk within 15 m.
fs::Spot open_spot(const fs::GameState& st) {
  const fs::ArmyState& a = st.army;
  const double fx = lm::sin(a.pad_heading);
  const double fz = lm::cos(a.pad_heading);
  for (int32_t i = 0; i < 40; ++i) {
    const double r = 350 + 10.0 * i;
    for (const double side : {0.0, 60.0, -60.0, 120.0, -120.0}) {
      const double x = a.pad_x + fx * r + fz * side;
      const double z = a.pad_z + fz * r - fx * side;
      bool clear = true;
      for (const mg::TreeDef& t : st.level->trees) {
        if (dist(t.x - x, t.z - z) < 15 + t.radius) clear = false;
      }
      if (clear) return {x, 0, z};
    }
  }
  return {a.pad_x + fx * 350, 0, a.pad_z + fz * 350};
}

// A hunt on seed 38: he stands on the open spot at three stars for `hunted` s, the stars pinned
// and a helicopter over him; then the hunt is called off and the run goes on `after` s. What the
// machines did, minute by half minute.
struct Half {
  uint64_t hash = 0;
  int32_t out = 0;
  int32_t halted = 0;
  int32_t empty = 0;
};
struct Hunt {
  fs::Spot q;
  std::vector<fs::GameEvent> events;
  std::vector<Half> halves;
  double first_out = -1;     // s, the first machine out of the village
  double nearest = 1e9;      // m, the nearest any machine came while the hunt ran
  double halt_least = 1e9;   // m, the nearest a machine halted
  bool halted_side = false;  // a halt brought him off its nose to the right, past half its side
  int32_t written = 0;       // the crews in the pursuers at the end of the hunt
  int32_t crewed = 0;        // ...and the machines out with their crews
  int32_t left_after = 0;    // machines still out at the end
  double arrested_at = -1;
  bool by_crew = false;
};

Hunt hunt(bool lethal, double hunted, double after) {
  fs::GameState st = fs::create_free_ride(seed38(), 0);
  // Staged: the snowmobiles alone: the helicopters are fall_line_army_heli_test's.
  st.army.send_helis = false;
  st.army.fire = false;
  Hunt h;
  const fs::Spot at = open_spot(st);
  st.racers[0].skier = sk::stand_skier(st.world(), at.x, at.z, 0, 0);
  st.wanted.lethal = lethal;
  fs::set_wanted(st, 3);
  h.q = fs::quarry_of(st);
  const fs::Controls none{};
  const auto steps = static_cast<int64_t>(std::llround((hunted + after) * fs::kHz));
  for (int64_t i = 0; i < steps; i++) {
    const bool on = st.t < hunted;
    fs::WantedState& w = st.wanted;
    if (on && w.ended == fs::WantedEnd::running && w.stars > 0) {
      w.heat = 3;
      std::erase_if(w.pursuers, [](const fs::Pursuer& p) { return p.id == 1; });
      w.pursuers.push_back(fs::Pursuer{1, fs::PursuerKind::helicopter, h.q.x, h.q.y + 150, h.q.z});
    }
    if (!on && w.ended == fs::WantedEnd::running && w.stars > 0) {
      h.written = 0;
      for (const fs::Pursuer& p : w.pursuers) {
        h.written += p.id >= as::kPursuerBase && p.id < as::kPursuerBase + 1000 ? 1 : 0;
      }
      h.crewed = 0;
      for (const fs::ArmySled& m : st.army.sleds) {
        h.crewed += m.mode == fs::ArmySledMode::drive || m.mode == fs::ArmySledMode::halt;
      }
      fs::set_wanted(st, 0);
    }
    st.racers[0].skier = sk::stand_skier(st.world(), at.x, at.z, 0, 0);
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (const fs::GameEvent& e : st.events) {
      if (e.kind == fs::EventKind::army && fs::army_change(e) == fs::ArmyChange::arrest) {
        h.arrested_at = st.t;
        for (const fs::Soldier& s : st.army.soldiers) {
          if (s.id == st.army.arrested_by) h.by_crew = s.from == fs::SoldierFrom::sled;
        }
      }
      if (e.kind != fs::EventKind::army_sled) continue;
      h.events.push_back(e);
      const auto c = static_cast<fs::ArmySledChange>(e.gate);
      if (c == fs::ArmySledChange::out && h.first_out < 0) h.first_out = st.t;
    }
    for (const fs::ArmySled& m : st.army.sleds) {
      if (m.mode == fs::ArmySledMode::due || !on) continue;
      const double d = dist(m.body.x - h.q.x, m.body.z - h.q.z);
      h.nearest = d < h.nearest ? d : h.nearest;
      // Settled in its halt: braked, the gunner on him.
      if (m.mode == fs::ArmySledMode::halt && m.t > as::kSettleS * 2 && m.body.speed < 1) {
        h.halt_least = d < h.halt_least ? d : h.halt_least;
        const double off =
            angle_diff(m.body.heading, lm::atan2(h.q.x - m.body.x, h.q.z - m.body.z));
        h.halted_side = h.halted_side || off > as::kSide * 0.5;
      }
    }
    if (st.steps % static_cast<int64_t>(30 * fs::kHz) == 0) {
      Half x;
      x.hash = fs::state_hash(st, 0xcbf29ce484222325ull);
      for (const fs::ArmySled& m : st.army.sleds) {
        x.out += m.mode != fs::ArmySledMode::due ? 1 : 0;
        x.halted += m.mode == fs::ArmySledMode::halt ? 1 : 0;
        x.empty += m.mode == fs::ArmySledMode::empty ? 1 : 0;
      }
      h.halves.push_back(x);
    }
    if (h.arrested_at >= 0 && st.t > h.arrested_at + 2) break;
  }
  h.left_after = static_cast<int32_t>(st.army.sleds.size());
  return h;
}

const Hunt& lethal_hunt() {
  static const Hunt h = hunt(true, 150, 150);
  return h;
}

int count(const std::vector<fs::GameEvent>& events, fs::ArmySledChange c) {
  int n = 0;
  for (const auto& e : events) n += static_cast<fs::ArmySledChange>(e.gate) == c ? 1 : 0;
  return n;
}

}  // namespace

TEST(the_way_read_goes_round_a_wall_of_trunks_and_straight_on_open_snow) {
  mg::Level open = sk::flat_strip(0, 1);
  const sk::TreeIndex none = sk::build_tree_index(open);
  const fs::WayChoice straight = fs::choose_way(open, none, 2048, 1000, 0, 0, read());
  CHECK(std::fabs(straight.way) < 1e-9);
  CHECK(!straight.woods);
  CHECK(straight.reach == as::kLookM);
  // A belt of woods across the way from 12 m ahead, three rows deep: it is steered off to a side,
  // round its end.
  mg::Level walled = sk::flat_strip(0, 1);
  for (int row = 0; row < 3; row++) {
    for (int i = -6; i <= 6; i++) {
      mg::TreeDef t{};
      t.x = 2048.0 + i * 2.0;
      t.z = 1012.0 + row * 6;
      t.height = 12;
      t.radius = 0.3;
      walled.trees.push_back(t);
    }
  }
  const sk::TreeIndex trees = sk::build_tree_index(walled);
  const fs::WayChoice round = fs::choose_way(walled, trees, 2048, 1000, 0, 0, read());
  CHECK(std::fabs(round.way) > 0.5);
}

TEST(a_face_steeper_than_it_climbs_is_taken_across_not_straight_up) {
  // The strip falls to +z at a grade past `kRise`: straight up it (-z) the way is stopped at once.
  const double pitch = lm::atan(as::kRise * 1.3);
  mg::Level face = sk::flat_strip(pitch, 1);
  const sk::TreeIndex none = sk::build_tree_index(face);
  const double up = ge::core::kPi;
  const fs::WayCost direct = fs::way_cost(face, none, 2048, 2000, up, up, up, read());
  CHECK(direct.reach < as::kBlockedM);
  const fs::WayChoice c = fs::choose_way(face, none, 2048, 2000, up, up, read());
  CHECK(std::fabs(angle_diff(c.way, up)) > 0.5);
  // A grade it climbs is taken straight up.
  mg::Level gentle = sk::flat_strip(lm::atan(as::kRise * 0.6), 1);
  const fs::WayChoice g = fs::choose_way(gentle, none, 2048, 2000, up, up, read());
  CHECK(std::fabs(angle_diff(g.way, up)) < 1e-9);
}

TEST(the_wind_tunnels_are_a_wall) {
  const mg::Level& level = *seed38();
  CHECK(level.resort.has_value());
  if (!level.resort || level.resort->tunnels.empty()) return;
  const mg::WindTunnel& t = level.resort->tunnels.front();
  const mg::WindTunnel::Point& p = t.points[t.points.size() / 2];
  CHECK(fs::tunnel_within(level, p.x, p.z, 0));
  CHECK(!fs::tunnel_within(level, p.x + lm::cos(p.heading) * (t.width + 100),
                           p.z - lm::sin(p.heading) * (t.width + 100), as::kTunnelM));
}

TEST(three_stars_call_a_machine_out_of_the_village_that_halts_side_on_outside_its_stand_off) {
  const Hunt& h = lethal_hunt();
  // The roster's one at three stars, and the first wave's (60 s after the star) on top.
  CHECK(count(h.events, fs::ArmySledChange::called) >= fs::roster_of(3).snowmobiles);
  CHECK(h.first_out >= as::kRespondS);
  CHECK(h.first_out < as::kRespondS + 1);
  // It closes on him and halts with him in sight, side-on, never inside the stand-off.
  CHECK(count(h.events, fs::ArmySledChange::halt) > 0);
  CHECK(h.halted_side);
  CHECK(h.halt_least > as::kHaltLeastM);
  CHECK(h.nearest < as::kHaltRangeM);
  CHECK(h.nearest > as::kStandLethalM * 0.6);
  // Every machine out with its crew is a pursuer.
  CHECK(h.crewed > 0);
  CHECK(h.written == h.crewed);
  // Nothing went over or got caught on open snow.
  CHECK(count(h.events, fs::ArmySledChange::dug) == 0);
}

TEST(the_hunt_over_the_machines_drive_home_and_are_gone) {
  const Hunt& h = lethal_hunt();
  CHECK(count(h.events, fs::ArmySledChange::gone) > 0);
  CHECK(h.left_after == 0);
}

TEST(where_the_army_does_not_shoot_to_kill_a_crew_runs_him_down_and_takes_him_in) {
  const Hunt h = hunt(false, 200, 0);
  CHECK(count(h.events, fs::ArmySledChange::dismount) >= 1);
  CHECK(h.arrested_at > 0);
  CHECK(h.by_crew);
}

TEST(a_race_sends_no_machines) {
  fs::GameState st = fs::create_game_on(seed38(), 1);
  st.army.send_helis = false;
  st.army.fire = false;
  fs::set_wanted(st, 5);
  const fs::Controls none{};
  for (int64_t i = 0; i < std::llround(40 * fs::kHz); i++) {
    st.wanted.heat = 5;
    fs::step(st, std::span<const fs::Controls>(&none, 1));
  }
  CHECK(st.army.sleds.empty());
}

TEST(a_scripted_hunt_on_seed_38_keeps_its_golden) {
  const Hunt& h = lethal_hunt();
  std::vector<std::string> got;
  got.reserve(h.halves.size());
  for (const Half& x : h.halves) {
    got.push_back(hex(x.hash) + " " + std::to_string(x.out) + " " + std::to_string(x.halted) + " " +
                  std::to_string(x.empty));
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# each half minute: state digest, machines out, halted, empty "
           "(fall_line_army_sled_test, "
           "the seed 38 hunt at three stars for 150 s, then called off for 150 s)\n";
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
                                  " with GE_GOLDEN_WRITE=1 and put the army-sleds lab's table "
                                  "before and after in the PR (skill: sim).");
  }
}
