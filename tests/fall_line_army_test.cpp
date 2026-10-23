// FALL LINE'S ARMY ON FOOT, HELD (games/fall_line/sim/army.hpp, army_troop.hpp): a soldier's way
// over the snow (the skate, the pace, the traverse up the steep, round a trunk, the side-step out
// of where he is caught), a ground wave's column formed out of his sight and climbing at him, the
// arrest where the army does not shoot to kill and the stand-off where it does, the men sent home,
// the lifts' squads riding up on seed 38, and a scripted hunt there held bit for bit by
// tests/golden/fall_line_army.txt.
//
// The hunted man stands still; the stars are pinned where a scene needs them (his heat held each
// step) and a helicopter hovers over him so the hunt keeps sight of him. A deliberate change to
// the army's rules or numbers moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_army_test`, and put the `army` lab's table
// before and after in the PR.
//
// ge:summary Test: the army on foot's troop model, waves, lift squads, chase, arrest; golden
// ge:keywords test army soldier troop wave column lift-squad chase arrest stand-off sent-home
// ge:keywords golden digest traverse
// ge:skill sim testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/army.hpp"
#include "fall_line/sim/army_rules.hpp"
#include "fall_line/sim/army_troop.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "golden.hpp"
#include "skier/synthetic.hpp"
#include "skier/trees.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace ar = fall_line::sim::army;
namespace lm = ge::core::libm;
double dist(double a, double b) { return ge::core::hypot(a, b); }
constexpr const char* kGolden = "tests/golden/fall_line_army.txt";

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

// The drag strip on a grade of `pitch` rad (falling to +z), `trees` standing on it.
std::shared_ptr<const mg::Level> strip(double pitch = 0,
                                       const std::vector<mg::TreeDef>& trees = {}) {
  mg::Level level = sk::flat_strip(pitch, 1);
  level.trees = trees;
  return std::make_shared<const mg::Level>(std::move(level));
}

// One soldier alone on `level` at (x, z), facing `heading`, stepped by the troop model alone.
struct Drill {
  std::shared_ptr<const mg::Level> level;
  sk::TreeIndex trees;
  std::vector<fs::Soldier> men;
  double t = 0;

  Drill(std::shared_ptr<const mg::Level> l, double x, double z, double heading)
      : level(std::move(l)), trees(sk::build_tree_index(*level)) {
    fs::Soldier s;
    s.mode = fs::SoldierMode::chase;
    s.x = x;
    s.z = z;
    s.y = level->ground_at(x, z);
    s.heading = heading;
    men.push_back(s);
  }
  fs::TroopWorld world() const { return fs::TroopWorld{level.get(), &trees, men, t, fs::kDt}; }
  fs::Soldier& man() { return men[0]; }
  // Toward (tx, tz) for `seconds`, the stuck watch kept; returns the most he was caught in a row.
  int32_t go(double tx, double tz, double seconds, double pace = ar::kPace, double stop = -1) {
    int32_t worst = 0;
    for (int64_t i = 0; i < std::llround(seconds * fs::kHz); i++) {
      fs::go_toward(world(), man(), tx, tz, pace, stop);
      t += fs::kDt;
      const int32_t slow = fs::watch_progress(man(), t, true);
      worst = slow > worst ? slow : worst;
    }
    return worst;
  }
};

// Pins the stars at `stars` (his heat held) and keeps a helicopter 150 m over him.
void pin(fs::GameState& state, int32_t stars) {
  state.wanted.heat = stars;
  std::erase_if(state.wanted.pursuers, [](const fs::Pursuer& p) { return p.id == 1; });
  const fs::Spot q = fs::quarry_of(state);
  state.wanted.pursuers.push_back(fs::Pursuer{1, fs::PursuerKind::helicopter, q.x, q.y + 150, q.z});
}

// Steps the run with nothing pressed (he stands still, never reset for it) for `seconds`, the
// stars pinned at `stars` while the hunt runs (none: left to the hunt); returns every army event.
std::vector<fs::GameEvent> stand(fs::GameState& state, double seconds, int32_t stars) {
  std::vector<fs::GameEvent> all;
  const fs::Controls none{};
  for (int64_t i = 0; i < std::llround(seconds * fs::kHz); i++) {
    if (stars > 0 && state.wanted.ended == fs::WantedEnd::running && state.wanted.stars > 0) {
      pin(state, stars);
    }
    fs::step(state, std::span<const fs::Controls>(&none, 1));
    for (const fs::GameEvent& e : state.events) {
      if (e.kind == fs::EventKind::army) all.push_back(e);
    }
  }
  return all;
}

int men(const std::vector<fs::GameEvent>& events, fs::ArmyChange c) {
  int n = 0;
  for (const auto& e : events) n += fs::army_change(e) == c ? static_cast<int>(e.value) : 0;
  return n;
}

int count(const fs::GameState& state, fs::SoldierMode m) {
  int n = 0;
  for (const fs::Soldier& s : state.army.soldiers) n += s.mode == m ? 1 : 0;
  return n;
}

double nearest(const fs::GameState& state) {
  const fs::Spot q = fs::quarry_of(state);
  double d = 1e9;
  for (const fs::Soldier& s : state.army.soldiers) {
    const double e = dist(s.x - q.x, s.z - q.z);
    d = e < d ? e : d;
  }
  return d;
}

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(38, 10));
  return level;
}

}  // namespace

TEST(on_the_flat_he_skates_to_where_he_is_sent_and_stops_short_when_asked) {
  Drill d(strip(), 2048, 1000, 0);
  d.go(2048, 1060, 25, ar::kPace, 1);
  CHECK(std::fabs(d.man().z - 1059) < 0.5);
  CHECK(d.man().speed < 0.5);
  // On the flat the hill carries nobody: he works at his skating pace.
  Drill e(strip(), 2048, 1000, 0);
  e.go(2048, 2000, 6);
  CHECK(std::fabs(e.man().speed - ar::kSkate) < 0.1);
}

TEST(down_a_steep_slope_the_hill_carries_him_and_he_checks_it_to_his_pace) {
  Drill d(strip(0.35), 2048, 200, 0);
  d.go(2048, 3800, 20);
  CHECK(!d.man().climbing);
  CHECK(std::fabs(d.man().speed - ar::kPace) < 0.1);
}

TEST(up_a_gentle_rise_he_climbs_straight_and_up_a_steep_one_he_zig_zags_across_it) {
  // Gentle: straight up, slower than on the flat.
  Drill g(strip(0.1), 2048, 2000, ge::core::kPi);
  g.go(2048, 1000, 20);
  CHECK(std::fabs(g.man().x - 2048) < 1);
  CHECK(g.man().speed < ar::kSkate - 0.3);
  // Steep: across it, never straight up the fall line, turned the other way now and then, and
  // still gaining height.
  Drill s(strip(0.6), 2048, 2000, ge::core::kPi);
  const double y0 = s.man().y;
  int left = 0;
  int right = 0;
  for (int i = 0; i < 60; i++) {
    s.go(2048, 1000, 1);
    const double off = ge::core::angle_diff(ge::core::kPi, s.man().heading);
    CHECK(std::fabs(off) > 0.5);
    left += off < 0 ? 1 : 0;
    right += off > 0 ? 1 : 0;
  }
  CHECK(left > 10 && right > 10);
  CHECK(s.man().y > y0 + 10);
}

TEST(he_steers_round_a_trunk_in_his_way_and_never_stands_in_it) {
  const mg::TreeDef trunk{2048, 1040, 2000, 18, 0.4, 60, 3, mg::TreeKind::spruce, -1};
  Drill d(strip(0, {trunk}), 2048, 1000, 0);
  double closest = 1e9;
  for (int i = 0; i < 30 * 120; i++) {
    d.go(2048, 1080, 1.0 / 120, ar::kPace, 0.5);
    closest = std::min(closest, dist(d.man().x - trunk.x, d.man().z - trunk.z));
  }
  CHECK(closest >= trunk.radius + 0.4 - 1e-9);
  CHECK(dist(d.man().x - 2048, d.man().z - 1080) < 1.5);
}

TEST(a_man_caught_steps_off_to_a_side_and_the_other_side_the_next_time) {
  fs::Soldier s;
  s.id = 4;
  CHECK(fs::watch_progress(s, 0, true) == 0);
  CHECK(fs::watch_progress(s, ar::kWindowS, true) == 1);
  CHECK(s.progress.side == 1 && s.progress.until == ar::kWindowS + ar::kDetourS);
  CHECK(fs::watch_progress(s, 2 * ar::kWindowS, true) == 2);
  CHECK(s.progress.side == -1);
  s.x = 10;
  CHECK(fs::watch_progress(s, 3 * ar::kWindowS, true) == 0);
  // Not meant to be moving: never caught.
  CHECK(fs::watch_progress(s, 9 * ar::kWindowS, false) == 0);
}

TEST(a_run_never_hunted_sends_nobody_and_hashes_no_army) {
  fs::GameState state = fs::create_game_on(strip(), 1);
  // Staged: the army on foot alone: its helicopters are fall_line_army_heli_test's.
  state.army.send_helis = false;
  state.army.fire = false;
  const uint64_t h0 = fs::state_hash(state, 1469598103934665603ull);
  fs::GameState other = state;
  other.army.waves = 7;  // unhashed while inactive
  CHECK(fs::state_hash(other, 1469598103934665603ull) == h0);
  stand(state, 30, 0);
  CHECK(!state.army.active);
  CHECK(state.army.soldiers.empty());
}

TEST(a_wave_forms_up_out_of_his_sight_down_the_piste_and_climbs_to_its_stand_off) {
  fs::GameState state = fs::create_game_on(strip(), 1);
  state.army.send_helis = false;
  state.army.fire = false;
  fs::set_wanted(state, 4);
  const fs::Spot q = fs::quarry_of(state);
  // Four stars send their first wave after `first_s`: its men on foot, all of them.
  const double first = fs::wanted::kWaves[4].first_s;
  CHECK(stand(state, first - 1, 4).empty());
  const auto events = stand(state, 2, 4);
  const int sent = men(events, fs::ArmyChange::wave);
  const std::optional<fs::WaveMix> wave = fs::ground_wave(4, 0);
  CHECK(wave.has_value());
  CHECK(wave && sent == wave->foot);
  CHECK(static_cast<int>(state.army.soldiers.size()) == sent);
  for (const fs::Soldier& s : state.army.soldiers) {
    CHECK(s.mode == fs::SoldierMode::climb);
    CHECK(s.from == fs::SoldierFrom::wave);
    // Down the piste from him, on it, and out of his reach.
    CHECK(s.z > q.z + ar::kAwayM);
    CHECK(std::fabs(s.x - sk::kStripSize / 2) < 30);
    CHECK(dist(s.x - q.x, s.z - q.z) >= ar::kAwayM);
  }
  // Every man who can look is one of the hunt's pursuers.
  int written = 0;
  for (const fs::Pursuer& p : state.wanted.pursuers) {
    written += p.id >= ar::kPursuerBase && p.kind == fs::PursuerKind::soldier ? 1 : 0;
  }
  CHECK(written == sent);
  // They climb at him and, the army shooting to kill, hold with him in sight within their
  // stand-off: nobody runs him down, nobody is caught on the way.
  const double before = nearest(state);
  stand(state, 240, 4);
  CHECK(nearest(state) < before - 100);
  CHECK(nearest(state) > ar::kStandOffM - 20);
  CHECK(count(state, fs::SoldierMode::hold) == static_cast<int>(state.army.soldiers.size()));
  for (const fs::Soldier& s : state.army.soldiers) CHECK(s.progress.slow < 2);
  CHECK(state.wanted.ended == fs::WantedEnd::running);
}

TEST(where_they_do_not_shoot_to_kill_they_run_him_down_and_arrest_him) {
  fs::GameState state = fs::create_game_on(strip(), 1);
  state.army.send_helis = false;
  state.army.fire = false;
  state.wanted.lethal = false;
  fs::set_wanted(state, 4);
  std::vector<fs::GameEvent> events;
  for (int i = 0; i < 300 && state.wanted.ended == fs::WantedEnd::running; i++) {
    const auto e = stand(state, 1, 4);
    events.insert(events.end(), e.begin(), e.end());
  }
  CHECK(state.wanted.ended == fs::WantedEnd::arrested);
  CHECK(men(events, fs::ArmyChange::arrest) == 1);
  CHECK(state.army.arrested_by >= 0);
  CHECK(count(state, fs::SoldierMode::arrest) == 1);
  CHECK(nearest(state) <= fs::wanted::kArrestReachM);
  // Taken in: the others stand where they are.
  stand(state, 5, 0);
  for (const fs::Soldier& s : state.army.soldiers) CHECK(s.speed == 0);
}

TEST(the_hunt_over_the_men_far_from_him_are_sent_home_and_the_rest_stand) {
  fs::GameState state = fs::create_game_on(strip(), 1);
  state.army.send_helis = false;
  state.army.fire = false;
  fs::set_wanted(state, 4);
  stand(state, fs::wanted::kWaves[4].first_s + 1, 4);
  const auto out = static_cast<int>(state.army.soldiers.size());
  CHECK(out > 0);
  // Escaped near them: they stop and stand.
  fs::GameState near_him = state;
  fs::set_wanted(near_him, 0);
  CHECK(near_him.wanted.ended == fs::WantedEnd::escaped);
  stand(near_him, 5, 0);
  CHECK(static_cast<int>(near_him.army.soldiers.size()) == out);
  for (const fs::Soldier& s : near_him.army.soldiers) CHECK(s.speed == 0);
  // Escaped far from them: every one sent home.
  const fs::Spot q = fs::quarry_of(state);
  state.racers[0].skier = sk::stand_skier(state.world(), q.x - 800, q.z, 0, 0);
  fs::set_wanted(state, 0);
  const auto events = stand(state, 1, 0);
  CHECK(men(events, fs::ArmyChange::home) == out);
  CHECK(state.army.soldiers.empty());
  for (const fs::Pursuer& p : state.wanted.pursuers) CHECK(p.id < ar::kPursuerBase);
}

namespace {

// THE SCRIPTED HUNT on seed 38: he stands 40 m beside the bottom station of the ski area's first
// lift, at three stars, with the army shooting to kill, for four minutes; what it looks like at
// each minute. Shared by the tests below (built once).
struct Minute {
  uint64_t hash;
  int out;
  int riding;
  int holding;
};
struct Seed38Hunt {
  fs::Spot q;
  int32_t lift = -1;
  int squads = 0;
  int sent = 0;
  int waves = 0;
  int off = 0;
  int written = 0;
  std::vector<Minute> minutes;
};

const Seed38Hunt& seed38_hunt() {
  static const Seed38Hunt hunt = [] {
    Seed38Hunt h;
    fs::GameState state = fs::create_free_ride(seed38(), 0);
    state.army.send_helis = false;
    state.army.fire = false;
    const auto& p = (*state.lifts)[0];
    state.racers[0].skier =
        sk::stand_skier(state.world(), p.bottom.x - p.dz * 40, p.bottom.z + p.dx * 40, 0, 0);
    fs::set_wanted(state, 3);
    h.q = fs::quarry_of(state);
    h.lift = fs::lift_for(state, h.q);
    for (int m = 0; m < 4; m++) {
      const auto events = stand(state, 60, 3);
      for (const fs::GameEvent& e : events) {
        if (fs::army_change(e) == fs::ArmyChange::squad) h.squads++;
      }
      h.sent += men(events, fs::ArmyChange::squad);
      h.waves += men(events, fs::ArmyChange::wave);
      h.off += men(events, fs::ArmyChange::off);
      h.minutes.push_back(Minute{fs::state_hash(state, 1469598103934665603ull),
                                 static_cast<int>(state.army.soldiers.size()),
                                 count(state, fs::SoldierMode::ride),
                                 count(state, fs::SoldierMode::hold)});
    }
    for (const fs::Pursuer& ps : state.wanted.pursuers) h.written += ps.id >= ar::kPursuerBase;
    return h;
  }();
  return hunt;
}

}  // namespace

TEST(on_seed_38_a_squad_rides_up_the_lift_whose_top_is_over_him_and_skis_down_at_him) {
  const Seed38Hunt& h = seed38_hunt();
  const fs::GameState state = fs::create_free_ride(seed38(), 0);
  // The lift taken has its top well over him and is the one whose top is nearest such.
  CHECK(h.lift >= 0);
  const auto& plan = (*state.lifts)[static_cast<size_t>(h.lift)];
  CHECK(state.level->ground_at(plan.top.x, plan.top.z) >= h.q.y + ar::kAboveM);
  for (const auto& other : *state.lifts) {
    if (state.level->ground_at(other.top.x, other.top.z) < h.q.y + ar::kAboveM) continue;
    CHECK(dist(other.top.x - h.q.x, other.top.z - h.q.z) >=
          dist(plan.top.x - h.q.x, plan.top.z - h.q.z));
  }
  // Three stars ask for their lifted men: one squad, all of whom ride up and are let off.
  CHECK(h.squads == 1);
  CHECK(h.sent == fs::roster_of(3).lifted);
  CHECK(h.off == h.sent);
  CHECK(h.minutes.back().riding == 0);
  // The ground waves came too; at the end nearly every man holds him in sight (a fold of the
  // ground may hide him from one), and every man is a pursuer.
  CHECK(h.waves > 0);
  CHECK(h.minutes.back().out == h.sent + h.waves);
  CHECK(h.minutes.back().holding * 4 >= h.minutes.back().out * 3);
  CHECK(h.written == h.minutes.back().out);
}

TEST(a_scripted_hunt_on_seed_38_keeps_its_golden) {
  const Seed38Hunt& h = seed38_hunt();
  std::vector<std::string> got;
  got.reserve(h.minutes.size() + 1);
  for (const Minute& x : h.minutes) {
    got.push_back(hex(x.hash) + " " + std::to_string(x.out) + " " + std::to_string(x.riding) + " " +
                  std::to_string(x.holding));
  }
  got.push_back(std::to_string(h.lift) + " " + std::to_string(h.sent) + " " +
                std::to_string(h.waves) + " " + std::to_string(h.off));
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# each minute: state digest, men out, riding, holding; then the lift taken, the lifted "
           "men sent, the waves' men, the men let off (fall_line_army_test, the seed 38 hunt)\n";
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
                                  " with GE_GOLDEN_WRITE=1 and put the army lab's table before "
                                  "and after in the PR (skill: sim).");
  }
}
