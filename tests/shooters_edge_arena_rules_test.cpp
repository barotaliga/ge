// Tests: the arena in C++ (games/shooters_edge/rules/arena*.cpp) plays the compiled QC arena's
// matches side by side: every player's path and frags each frame, every mover, every print and
// frag line, every hook call, the builtins' counts (but the text calls the QC needed to read a
// number, which C++ reads itself) and the loop's digest every frame. The matches: a deathmatch on
// the baseline arena with its door and plat to the time limit, the same in deathmatch 4, and one
// in a loaded game's words with teams.
//
// ge:summary Tests: the arena's rules in C++ play the compiled QC arena's matches to the bit
// ge:keywords test arena native-arena side-by-side parity baseline-mod mod-match deathmatch
// ge:keywords dmm4 words loop-digest
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/file.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/profiles/words.hpp"
#include "shooters_edge/rules/arena.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace pf = shooters_edge::profiles;

namespace {

const pg::Progs& compiled() {
  static const pg::Progs p = [] {
    pg::Progs out;
    std::string error;
    if (!sv::compile_baseline_mod(GE_SOURCE_DIR, out, error)) out = {};
    return out;
  }();
  return p;
}

sv::ModMatchReport play(bool native, const std::vector<std::string>& console) {
  sv::ModMatchOptions o;
  if (native) {
    o.native = &sv::native_arena();
  } else {
    o.compiled = &compiled();
    o.compiled_name = "the arena in QC";
  }
  o.entities = sv::kBaselineArenaEntities;
  o.bots = 2;
  o.max_frames = 5100;
  o.console = console;
  return sv::run_mod_match(o);
}

// The text calls the QC made to read a number (C++ reads the text itself).
bool counted(int number) { return number != 114 && number != 116; }

void same_match(const std::vector<std::string>& console) {
  const sv::ModMatchReport a = play(false, console);
  const sv::ModMatchReport n = play(true, console);
  CHECK_MSG(a.ok, a.error);
  CHECK_MSG(n.ok, n.error);
  CHECK_MSG(n.faults == 0, n.last_fault);
  CHECK(a.frames > 1000);
  CHECK(n.frames == a.frames);
  CHECK(n.ended == a.ended);
  CHECK(n.players.size() == a.players.size());
  for (size_t p = 0; p < a.players.size() && p < n.players.size(); p++) {
    const sv::PlayerPath& x = a.players[p];
    const sv::PlayerPath& y = n.players[p];
    CHECK_MSG(x.frags == y.frags && x.deaths == y.deaths && x.frags_at == y.frags_at, x.name);
    CHECK(x.points.size() == y.points.size());
    for (size_t i = 0; i < x.points.size() && i < y.points.size(); i++) {
      if (x.points[i].x == y.points[i].x && x.points[i].y == y.points[i].y &&
          x.points[i].z == y.points[i].z)
        continue;
      CHECK_MSG(false, x.name + " parts at frame " + std::to_string(i));
      break;
    }
  }
  CHECK(n.missiles.size() == a.missiles.size());
  CHECK(n.movers.size() == a.movers.size());
  for (size_t m = 0; m < a.movers.size() && m < n.movers.size(); m++)
    CHECK_MSG(n.movers[m].z == a.movers[m].z, a.movers[m].classname);
  CHECK(n.prints == a.prints);
  CHECK(n.frag_log == a.frag_log);
  CHECK(n.hook_log.size() == a.hook_log.size());
  for (int h = 0; h < static_cast<int>(sv::HookId::count); h++)
    CHECK(n.hook_counts[h] == a.hook_counts[h]);
  std::vector<sv::BuiltinCount> x;
  std::vector<sv::BuiltinCount> y;
  for (const sv::BuiltinCount& b : a.builtins)
    if (counted(b.number)) x.push_back(b);
  for (const sv::BuiltinCount& b : n.builtins)
    if (counted(b.number)) y.push_back(b);
  CHECK(x.size() == y.size());
  for (size_t i = 0; i < x.size() && i < y.size(); i++)
    CHECK_MSG(x[i].number == y[i].number && x[i].calls == y[i].calls,
              x[i].name + ": " + std::to_string(x[i].calls) + " calls in QC, " +
                  std::to_string(y[i].calls) + " in C++");
  // The world as played, frame by frame (the programs' memories differ: the QC keeps its
  // constants and locals in globals, and its strings sit elsewhere in its table).
  CHECK(n.loop_digests == a.loop_digests);
  for (size_t i = 0; i < a.loop_digests.size() && i < n.loop_digests.size(); i++) {
    if (a.loop_digests[i] == n.loop_digests[i]) continue;
    CHECK_MSG(false, "the world parts at frame " + std::to_string(i));
    break;
  }
  CHECK(a.frag_log.size() > 3);  // the match had kills to compare
}

}  // namespace

TEST(the_arena_in_cpp_plays_the_qc_deathmatch) {
  CHECK(!compiled().functions.empty());
  same_match({"deathmatch 1", "timelimit 1"});
}

TEST(the_arena_in_cpp_plays_the_qc_deathmatch_4) { same_match({"deathmatch 4", "timelimit 1"}); }

TEST(the_arena_in_cpp_plays_the_qc_match_in_a_games_words) {
  std::vector<std::string> console = pf::item_name_lines(pf::kClassicNames);
  for (std::string& line : pf::word_lines(pf::kClassicServerWords)) console.push_back(line);
  console.insert(console.end(), {"deathmatch 1", "teamplay 1", "timelimit 1"});
  same_match(console);
}
