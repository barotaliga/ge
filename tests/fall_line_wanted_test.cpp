// FALL LINE'S WANTED LEVEL, HELD (games/fall_line/sim/wanted.hpp): an offence's worth and floor,
// the climb in sight and at half pace searching, the stars dropped after the search, a hunt
// begun again, the sight line over a ridge and through the woods, cover from the air, the
// arrest, the roster and the ground waves, and a scripted hunt round a ridge held bit for bit by
// tests/golden/fall_line_wanted.txt.
//
// Every scene stands the player still on the flat drag strip (`flat_strip`), with the pursuers
// written by hand each step as the army's parts will write them. A deliberate change to the
// rules or the numbers moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/fall_line_wanted_test`, and put the `wanted` lab's
// table before and after in the PR.
//
// ge:summary Test: the wanted level's offences, climb, search, sight lines, cover, arrest; golden
// ge:keywords test wanted wanted-level stars heat climb search sight-line woods cover arrest
// ge:keywords roster ground-waves golden digest
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
#include "core/heightfield.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/wanted.hpp"
#include "golden.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace wd = fall_line::sim::wanted;
constexpr const char* kGolden = "tests/golden/fall_line_wanted.txt";

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

// The flat strip (level ground at 2000 m), with a ridge `ridge_m` high across it between
// x = 2100 and 2120 when asked for, and `trees` spruces standing on it.
std::shared_ptr<const mg::Level> strip(double ridge_m = 0,
                                       const std::vector<mg::TreeDef>& trees = {}) {
  mg::Level level = ge::skier::flat_strip(0, 1);
  if (ridge_m > 0) {
    ge::core::fill_field(level.ground, [&](double x, double) {
      return 2000 + (x >= 2100 && x <= 2120 ? ridge_m : 0);
    });
  }
  level.trees = trees;
  return std::make_shared<const mg::Level>(std::move(level));
}

mg::TreeDef spruce(double x, double z) {
  return mg::TreeDef{x, z, 2000, 18, 0.25, 60, 3, mg::TreeKind::spruce, -1};
}

// Steps the run with nothing pressed (he stands still on the flat), the pursuers handed in by
// `place` before every step; returns every event.
template <class F>
std::vector<fs::GameEvent> hunt(fs::GameState& state, double seconds, F&& place) {
  std::vector<fs::GameEvent> all;
  const fs::Controls none{};
  const auto steps = std::llround(seconds * fs::kHz);
  for (int64_t i = 0; i < steps; i++) {
    state.wanted.pursuers.clear();
    place(state);
    fs::step(state, std::span<const fs::Controls>(&none, 1));
    all.insert(all.end(), state.events.begin(), state.events.end());
  }
  return all;
}

void nobody(fs::GameState&) {}

// A helicopter hovering 150 m over him.
void overhead(fs::GameState& state) {
  const fs::Spot q = fs::quarry_of(state);
  state.wanted.pursuers.push_back(fs::Pursuer{1, fs::PursuerKind::helicopter, q.x, q.y + 150, q.z});
}

int changes(const std::vector<fs::GameEvent>& events, fs::WantedChange c) {
  int n = 0;
  for (const auto& e : events) n += e.kind == fs::EventKind::wanted && fs::wanted_change(e) == c;
  return n;
}

bool near(double a, double b, double tol) { return a > b - tol && a < b + tol; }

}  // namespace

TEST(an_offence_raises_the_heat_by_its_worth_to_at_least_its_floor) {
  fs::GameState state = fs::create_game_on(strip(), 1);
  // Staged: the scripted pursuers alone: the army's helicopters are fall_line_army_heli_test's.
  state.army.send_helis = false;
  state.army.send_vehicles = false;
  state.army.fire = false;
  CHECK(!state.wanted.active);
  CHECK(fs::raise_heat(state, fs::WantedCause::stole_ambulance) == 1);
  CHECK(state.wanted.active);
  CHECK(changes(state.events, fs::WantedChange::raised) == 1);
  CHECK(changes(state.events, fs::WantedChange::seen) == 1);
  CHECK(state.wanted.fix == wd::kFixS);
  // One more than he had, or the floor where it is higher.
  CHECK(fs::raise_heat(state, fs::WantedCause::stole_police) == 2);
  CHECK(fs::raise_heat(state, fs::WantedCause::downed_helicopter) == 3);
  CHECK(fs::raise_heat(state, fs::WantedCause::hit_soldier) == 3);
  CHECK(state.wanted.heat == 3.5);
  CHECK(fs::raise_heat(state, fs::WantedCause::stole_tank) == 5);
  CHECK(fs::raise_heat(state, fs::WantedCause::stole_tank) == 5);
  CHECK(state.wanted.peak == 5);
  CHECK(state.wanted.cause == fs::WantedCause::stole_tank);
  // By hand: down to two is a lowering, to none a loss.
  fs::set_wanted(state, 2);
  CHECK(state.wanted.stars == 2);
  CHECK(changes(state.events, fs::WantedChange::lowered) == 1);
  fs::set_wanted(state, 0);
  CHECK(state.wanted.stars == 0 && state.wanted.ended == fs::WantedEnd::escaped);
}

TEST(seen_the_hunt_climbs_a_star_every_climb_and_never_searches) {
  fs::GameState state = fs::create_game_on(strip(), 1);
  state.army.send_helis = false;
  state.army.send_vehicles = false;
  state.army.fire = false;
  fs::raise_heat(state, fs::WantedCause::stole_ambulance);
  const auto events = hunt(state, 40 + 50 + 1, overhead);
  CHECK(changes(events, fs::WantedChange::searching) == 0);
  CHECK(state.wanted.stars == 3);
  CHECK_MSG(near(state.wanted.reached[2], 40, 2 * fs::kDt),
            "star 2 at " + std::to_string(state.wanted.reached[2]) + " s, want 40 s");
  CHECK_MSG(near(state.wanted.reached[3], 90, 2 * fs::kDt),
            "star 3 at " + std::to_string(state.wanted.reached[3]) + " s, want 90 s");
  CHECK(state.wanted.seen);
  CHECK(fs::survived_of(state.wanted, state.t) == state.t);
}

TEST(unseen_the_stars_flash_after_the_fix_and_drop_once_the_search_runs_out) {
  fs::GameState state = fs::create_game_on(strip(), 1);
  state.army.send_helis = false;
  state.army.send_vehicles = false;
  state.army.fire = false;
  fs::raise_heat(state, fs::WantedCause::stole_ambulance);
  std::vector<fs::GameEvent> events = hunt(state, wd::kFixS + 1, nobody);
  CHECK(!state.wanted.seen);
  CHECK(changes(events, fs::WantedChange::searching) == 1);
  events = hunt(state, wd::kLoseS[1], nobody);
  CHECK(changes(events, fs::WantedChange::lost) == 1);
  CHECK(state.wanted.stars == 0);
  CHECK(state.wanted.ended == fs::WantedEnd::escaped);
  // The fix, the first look that missed him, then the search's length (within a look).
  const double survived = fs::survived_of(state.wanted, state.t);
  CHECK_MSG(near(survived, wd::kFixS + wd::kLoseS[1], wd::kLookEveryS + 2 * fs::kDt),
            "survived " + std::to_string(survived) + " s");
  // The timer stopped with the hunt.
  hunt(state, 5, nobody);
  CHECK(fs::survived_of(state.wanted, state.t) == survived);
  // Searching, the heat climbed at half pace: 1 + 10/40 + 12/2/40 is still one star.
  CHECK(changes(events, fs::WantedChange::raised) == 0);
}

TEST(a_hunt_escaped_begins_again_on_the_next_offence) {
  fs::GameState state = fs::create_game_on(strip(), 1);
  state.army.send_helis = false;
  state.army.send_vehicles = false;
  state.army.fire = false;
  fs::raise_heat(state, fs::WantedCause::stole_police);
  hunt(state, 60, nobody);
  CHECK(state.wanted.ended == fs::WantedEnd::escaped);
  const double t = state.t;
  CHECK(fs::raise_heat(state, fs::WantedCause::stole_ambulance) == 1);
  CHECK(state.wanted.ended == fs::WantedEnd::running);
  CHECK(state.wanted.since == t);
  CHECK(state.wanted.peak == 1);
}

TEST(the_ground_and_the_distance_hide_him_and_nearness_gives_him_away) {
  const auto level = strip(30);
  const ge::skier::TreeIndex trees = ge::skier::build_tree_index(*level);
  const fs::Spot q{2048, 2000, 500};
  // A soldier beyond the ridge cannot see over it; a helicopter high over it can.
  CHECK(
      !fs::sees_quarry(*level, trees, {1, fs::PursuerKind::soldier, 2200, 2001.7, 500}, q, false));
  CHECK(
      fs::sees_quarry(*level, trees, {1, fs::PursuerKind::helicopter, 2200, 2300, 500}, q, false));
  // On open snow each kind sees as far as its row: a soldier 350 m, a snowmobile's crew 400 m.
  CHECK(
      !fs::sees_quarry(*level, trees, {1, fs::PursuerKind::soldier, 2048, 2001.7, 880}, q, false));
  CHECK(fs::sees_quarry(*level, trees, {1, fs::PursuerKind::snowmobile, 2048, 2001.7, 880}, q,
                        false));
  // Within earshot he is found whatever is between.
  CHECK(fs::sees_quarry(*level, trees, {1, fs::PursuerKind::soldier, 2048, 1990, 520}, q, false));
}

TEST(three_crowns_hide_him_from_the_snow_but_not_from_the_air) {
  const fs::Spot q{2048, 2000, 500};
  const auto two = strip(0, {spruce(2048.5, 540), spruce(2047.5, 580)});
  const auto three = strip(0, {spruce(2048.5, 540), spruce(2047.5, 580), spruce(2048, 620)});
  const ge::skier::TreeIndex trees2 = ge::skier::build_tree_index(*two);
  const ge::skier::TreeIndex trees3 = ge::skier::build_tree_index(*three);
  const fs::Pursuer soldier{1, fs::PursuerKind::soldier, 2048, 2001.7, 700};
  const fs::Pursuer heli{2, fs::PursuerKind::helicopter, 2048, 2150, 700};
  CHECK(fs::sees_quarry(*two, trees2, soldier, q, false));
  CHECK(!fs::sees_quarry(*three, trees3, soldier, q, false));
  CHECK(fs::sees_quarry(*three, trees3, heli, q, false));
  CHECK(
      fs::through_woods(*three, trees3, fs::Spot{2048, 2001.7, 700}, fs::Spot{2048, 2001.1, 500}));
}

TEST(standing_under_a_crown_hides_him_from_the_air_beyond_its_reach) {
  fs::GameState open = fs::create_game_on(strip(), 1);
  open.army.send_helis = false;
  open.army.send_vehicles = false;
  open.army.fire = false;
  CHECK(!fs::under_cover(open));
  const fs::Spot at = fs::quarry_of(open);
  fs::GameState state = fs::create_game_on(strip(0, {spruce(at.x + 0.6, at.z)}), 1);
  state.army.send_helis = false;
  state.army.send_vehicles = false;
  state.army.fire = false;
  CHECK(fs::under_cover(state));
  const fs::Spot q = fs::quarry_of(state);
  // A crew 150 m over him cannot see through the boughs; nearer than the cover's reach they do.
  CHECK(!fs::sees_quarry(*state.level, state.trees,
                         {1, fs::PursuerKind::helicopter, q.x, q.y + 150, q.z}, q, true));
  CHECK(fs::sees_quarry(*state.level, state.trees,
                        {1, fs::PursuerKind::helicopter, q.x, q.y + 40, q.z}, q, true));
  // A soldier on the snow sees under one tree.
  CHECK(fs::sees_quarry(*state.level, state.trees,
                        {1, fs::PursuerKind::soldier, q.x + 100, q.y + 1.7, q.z}, q, true));
  // Hidden from the one helicopter out, the search runs out under the tree.
  fs::raise_heat(state, fs::WantedCause::stole_ambulance);
  hunt(state, wd::kFixS + wd::kLoseS[1] + 1, overhead);
  CHECK(state.wanted.ended == fs::WantedEnd::escaped);
}

TEST(the_arrest_ends_the_hunt_only_where_the_army_does_not_shoot_to_kill) {
  fs::GameState state = fs::create_game_on(strip(), 1);
  state.army.send_helis = false;
  state.army.send_vehicles = false;
  state.army.fire = false;
  CHECK(!fs::arrest_quarry(state));
  fs::raise_heat(state, fs::WantedCause::stole_ambulance);
  CHECK(!fs::arrest_quarry(state));
  state.wanted.lethal = false;
  hunt(state, 5, overhead);
  CHECK(fs::arrest_quarry(state));
  CHECK(state.wanted.ended == fs::WantedEnd::arrested);
  CHECK(changes(state.events, fs::WantedChange::arrested) == 1);
  const double survived = fs::survived_of(state.wanted, state.t);
  hunt(state, 5, overhead);
  CHECK(fs::survived_of(state.wanted, state.t) == survived);
  // The run is over: nothing raises him again.
  CHECK(fs::raise_heat(state, fs::WantedCause::stole_tank) == 0);
  CHECK(state.wanted.ended == fs::WantedEnd::arrested);
}

TEST(each_star_sends_its_roster_and_its_ground_waves_grow_to_their_caps) {
  CHECK(fs::roster_of(0).helicopters == 0);
  CHECK(fs::roster_of(1).fire == fs::FireRule::hold);
  CHECK(fs::roster_of(2).fire == fs::FireRule::warn);
  CHECK(fs::roster_of(3).fire == fs::FireRule::aim && fs::roster_of(3).lifted == 4);
  CHECK(fs::roster_of(9).helicopters == 3);
  CHECK(!fs::ground_wave(1, 0).has_value());
  const auto first = fs::ground_wave(5, 0);
  const auto fourth = fs::ground_wave(4, 3);
  const auto late = fs::ground_wave(5, 100);
  CHECK(first && fourth && late);
  if (first && fourth && late) {
    CHECK(first->snowmobiles == 3 && first->tracked == 2 && first->trucks == 2 &&
          first->tanks == 1 && first->foot == 24);
    CHECK(fourth->snowmobiles == 3 && fourth->tanks == 0 && fourth->foot == 34);
    CHECK(late->snowmobiles == 6 && late->tracked == 6 && late->trucks == 4 && late->tanks == 3 &&
          late->foot == 64);
  }
  // Counted from the star: none before `first_s`, then one every `every_s`.
  fs::GameState state = fs::create_game_on(strip(), 1);
  state.army.send_helis = false;
  state.army.send_vehicles = false;
  state.army.fire = false;
  fs::set_wanted(state, 3);
  CHECK(fs::waves_sent(state.wanted, 59) == 0);
  CHECK(fs::waves_sent(state.wanted, 60) == 1);
  CHECK(fs::waves_sent(state.wanted, 179) == 2);
  CHECK(fs::waves_sent(state.wanted, 180) == 3);
}

TEST(a_scripted_hunt_round_a_ridge_keeps_its_golden) {
  // A helicopter flying a square round him, 40 m over the snow, a lap every 40 s, its near side
  // 80 m behind a 60 m ridge: seen, then searching for a side, the stars climbing, for four
  // minutes.
  fs::GameState state = fs::create_game_on(strip(60), 1);
  state.army.send_helis = false;
  state.army.send_vehicles = false;
  state.army.fire = false;
  fs::raise_heat(state, fs::WantedCause::stole_police);
  int64_t step = 0;
  const auto events = hunt(state, 240, [&step](fs::GameState& s) {
    const fs::Spot q = fs::quarry_of(s);
    const double k = static_cast<double>(step++ % 4800) / 4800;
    const double u = k < 0.25 ? k * 4 : k < 0.5 ? 1 : k < 0.75 ? 1 - (k - 0.5) * 4 : 0;
    const double v = k < 0.25 ? 0 : k < 0.5 ? (k - 0.25) * 4 : k < 0.75 ? 1 : 1 - (k - 0.75) * 4;
    s.wanted.pursuers.push_back(fs::Pursuer{1, fs::PursuerKind::helicopter, q.x - 700 + 780 * u,
                                            2040, q.z - 400 + 800 * v});
  });
  std::string row;
  for (const double r : state.wanted.reached) row += hex(ge::test::bits(r)) + " ";
  row += std::to_string(state.wanted.stars) + " " +
         std::string(fs::wanted_end_name(state.wanted.ended)) + " " +
         std::to_string(changes(events, fs::WantedChange::searching)) + " " +
         std::to_string(changes(events, fs::WantedChange::seen)) + " " +
         hex(fs::state_hash(state, 0xcbf29ce484222325ull));
  CHECK_MSG(changes(events, fs::WantedChange::searching) > 0, "the ridge never hid him");
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# reached-bits for stars 0..5, stars, end, searching, seen, state digest "
           "(fall_line_wanted_test, the ridge hunt)\n"
        << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  std::string line;
  if (!want.empty()) {
    for (const auto& w : want[0]) line += (line.empty() ? "" : " ") + w;
  }
  CHECK_MSG(line == row, "the ridge hunt moved:\n  got  " + row + "\n  want " + line +
                             "\nIf the move is meant, rewrite " + kGolden +
                             " with GE_GOLDEN_WRITE=1 and put the wanted lab's table before and "
                             "after in the PR (skill: sim).");
}
