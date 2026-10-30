// FALL LINE'S CROWD, HELD (games/fall_line/sim/crowd.hpp, crowd_lift.hpp): the deal (the count,
// the groups as their rules make them, nothing dealt in a race or when none is asked), the crowd's
// own stream (a free ride dealt none is the run it always was), the way down a run (on its snow,
// moving, no place that is not a number), the lifts (the queue, the carriers, let off at the top
// and onto a run), the player meeting an amateur (pushed apart, knocked down past the knock, a
// `bump` event), and seed 38's crowd over forty seconds held bit for bit by
// tests/golden/fall_line_crowd.txt.
//
// A deliberate change to the crowd's rules or numbers moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/debug/tests/fall_line_crowd_test`, and put the `crowd` lab's tables
// before and after in the PR.
//
// ge:summary Test: the crowd's deal, its stream, the runs, the lifts, the player met; golden
// ge:keywords test crowd amateur deal group stream lift queue ride bump knock golden digest
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
#include "core/math.hpp"
#include "fall_line/sim/crowd.hpp"
#include "fall_line/sim/crowd_lift.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "golden.hpp"
#include "skier/skier.hpp"

namespace {

namespace fs = fall_line::sim;
namespace fc = fall_line::sim::crowd;
namespace mg = ge::mountain::mapgen;
constexpr const char* kGolden = "tests/golden/fall_line_crowd.txt";

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

const std::shared_ptr<const mg::Level>& seed38() {
  static const auto level = std::make_shared<const mg::Level>(fs::free_map(38, 10));
  return level;
}

// A free ride on seed 38 dealt `count` amateurs.
fs::GameState ride_with(int32_t count) {
  fs::GameState st = fs::create_free_ride(seed38(), 0);
  fs::deal_crowd(st, count);
  return st;
}

// Step it `seconds` with the player stood where he started, every step.
void run_for(fs::GameState& st, double seconds) {
  const ge::skier::SkierState stood = st.racers[0].skier;
  const fs::Controls none{};
  const double until = st.t + seconds;
  while (st.t < until) {
    st.racers[0].skier = stood;
    fs::step(st, std::span<const fs::Controls>(&none, 1));
  }
}

TEST(the_deal_makes_the_count_in_groups_by_their_rules) {
  const fs::GameState st = ride_with(300);
  const fs::CrowdState& c = st.crowd;
  CHECK(c.active);
  CHECK(c.amateurs.size() == 300);
  size_t members = 0;
  for (const fs::CrowdGroup& g : c.groups) {
    members += g.members.size();
    const fc::GroupDef& def = fc::kGroups[static_cast<size_t>(g.kind)];
    CHECK(g.track == def.track);
    for (size_t k = 0; k < g.members.size(); k++) {
      const fs::Amateur& a = c.amateurs[static_cast<size_t>(g.members[k])];
      CHECK(a.group == static_cast<int32_t>(&g - c.groups.data()));
      CHECK(a.rank == static_cast<int32_t>(k));
      CHECK(a.knobs.skill >= 0 && a.knobs.skill <= 1);
    }
    // A family's first follower is a child; a school's children ski the instructor's turns.
    if (g.kind == fc::GroupKind::family && g.members.size() > 1) {
      CHECK(c.amateurs[static_cast<size_t>(g.members[1])].kind == fc::Kind::kid);
    }
    if (g.track) {
      const fs::Knobs& lead = c.amateurs[static_cast<size_t>(g.members[0])].knobs;
      for (const int32_t m : g.members) {
        CHECK(c.amateurs[static_cast<size_t>(m)].knobs.style == lead.style);
        CHECK(c.amateurs[static_cast<size_t>(m)].knobs.turn == lead.turn);
      }
    }
  }
  CHECK(members == c.amateurs.size());
}

TEST(no_crowd_is_dealt_in_a_race_or_when_none_is_asked) {
  fs::GameOptions opts;
  fs::GameState race = fs::create_game(opts);
  fs::deal_crowd(race, 200);
  CHECK(!race.crowd.active);
  const fs::GameState none = ride_with(0);
  CHECK(!none.crowd.active);
  CHECK(none.crowd.amateurs.empty());
}

TEST(a_free_ride_dealt_no_crowd_is_the_run_it_always_was) {
  fs::GameState plain = fs::create_free_ride(seed38(), 0);
  fs::GameState none = ride_with(0);
  run_for(plain, 5);
  run_for(none, 5);
  CHECK(fs::state_hash(plain, 0) == fs::state_hash(none, 0));
}

TEST(the_crowd_replays_bit_for_bit) {
  fs::GameState a = ride_with(200);
  fs::GameState b = ride_with(200);
  run_for(a, 20);
  run_for(b, 20);
  CHECK(fs::state_hash(a, 0) == fs::state_hash(b, 0));
}

TEST(two_minutes_on_seed_38_keep_everyone_on_the_snow_and_moving) {
  fs::GameState st = ride_with(200);
  const fs::CrowdState& c = st.crowd;
  std::vector<double> still(c.amateurs.size(), 0);
  int32_t rode = 0;
  int32_t skated_on = 0;
  std::vector<fs::AmateurMode> was(c.amateurs.size());
  for (size_t i = 0; i < c.amateurs.size(); i++) was[i] = c.amateurs[i].mode;
  const ge::skier::SkierState stood = st.racers[0].skier;
  const fs::Controls none{};
  double worst_still = 0;
  bool finite = true;
  bool on_snow = true;
  while (st.t < 120) {
    st.racers[0].skier = stood;
    fs::step(st, std::span<const fs::Controls>(&none, 1));
    for (size_t i = 0; i < c.amateurs.size(); i++) {
      const fs::Amateur& a = c.amateurs[i];
      finite = finite && std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
      rode += was[i] == fs::AmateurMode::ride && a.mode == fs::AmateurMode::skate ? 1 : 0;
      skated_on += was[i] == fs::AmateurMode::skate && a.mode == fs::AmateurMode::ski ? 1 : 0;
      was[i] = a.mode;
      if (a.mode == fs::AmateurMode::ski) {
        const fc::Along at = fc::sample_run((*st.runs)[static_cast<size_t>(a.run)], a.s);
        // On his run's snow, or being eased back onto it after a merge.
        on_snow = on_snow && std::abs(a.d) < at.width / 2 + 3;
        still[i] = a.speed < 0.3 ? still[i] + fs::kDt : 0;
        worst_still = still[i] > worst_still ? still[i] : worst_still;
      } else {
        still[i] = 0;
      }
    }
  }
  CHECK(finite);
  CHECK(on_snow);
  CHECK_MSG(worst_still < 30, "an amateur skied yet stood still for " +
                                  std::to_string(worst_still) + " s; run `make lab LAB=crowd`");
  CHECK_MSG(rode > 0, "nobody was let off a lift's top in two minutes");
  CHECK_MSG(skated_on > 0, "nobody skated off a lift's top onto a run");
}

TEST(the_player_shoulders_an_amateur_and_knocks_him_down_past_the_knock) {
  for (const double closing : {1.5, 8.0}) {
    fs::GameState st = ride_with(1);
    fs::Amateur& a = st.crowd.amateurs[0];
    // Him on a run, stood still; the player a body's width uphill of him, coming straight at him.
    a.mode = fs::AmateurMode::ski;
    a.run = 0;
    a.s = 100;
    a.d = 0;
    a.speed = 0;
    a.yaw = 0;
    fc::place_on_run(st, a);
    const fc::Along at = fc::sample_run((*st.runs)[0], a.s);
    ge::skier::SkierState& me = st.racers[0].skier;
    const double back = fc::kSizes[static_cast<size_t>(a.body)].radius + fc::kPlayerRadius - 0.2;
    me.x = a.x - std::sin(at.heading) * back;
    me.z = a.z - std::cos(at.heading) * back;
    me.y = a.y;
    me.vx = std::sin(at.heading) * closing;
    me.vz = std::cos(at.heading) * closing;
    st.events.clear();
    fs::meet_crowd(st);
    const double gap = ge::core::hypot(a.x - me.x, a.z - me.z);
    CHECK(gap >= fc::kSizes[static_cast<size_t>(a.body)].radius + fc::kPlayerRadius - 1e-9);
    CHECK(a.speed > 0);
    const double knock = fc::kKnock.lo + (fc::kKnock.hi - fc::kKnock.lo) * a.knobs.skill;
    CHECK((a.mode == fs::AmateurMode::down) == (closing >= knock));
    bool bumped = false;
    for (const fs::GameEvent& e : st.events) bumped = bumped || e.kind == fs::EventKind::bump;
    CHECK(bumped);
  }
}

TEST(seed_38s_crowd_keeps_its_golden) {
  fs::GameState st = ride_with(300);
  std::vector<std::string> got;
  for (int k = 0; k < 4; k++) {
    run_for(st, 10);
    int32_t modes[7] = {};
    for (const fs::Amateur& a : st.crowd.amateurs) modes[static_cast<size_t>(a.mode)]++;
    std::string row = hex(fs::state_hash(st, 0)) + " " + fs::state_parts(st);
    for (const int32_t m : modes) row += " " + std::to_string(m);
    got.push_back(row);
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# every 10 s of seed 38's free ride dealt 300 amateurs, the player stood still: the "
           "state digest and its parts, then how many ski, stop, are down, queue, ride, skate and "
           "are up a lift "
           "unseen (fall_line_crowd_test)\n";
    for (const std::string& row : got) out << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  CHECK(want.size() == got.size());
  for (size_t r = 0; r < got.size() && r < want.size(); r++) {
    std::string line;
    for (const auto& w : want[r]) line += (line.empty() ? "" : " ") + w;
    CHECK_MSG(line == got[r], "seed 38's crowd moved at row " + std::to_string(r) + ":\n  got  " +
                                  got[r] + "\n  want " + line + "\nIf the move is meant, rewrite " +
                                  kGolden +
                                  " with GE_GOLDEN_WRITE=1 and put the crowd lab's tables before "
                                  "and after in the PR (skill: sim).");
  }
}

}  // namespace
