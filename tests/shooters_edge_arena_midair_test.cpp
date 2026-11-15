// MIDAIR ON THE ARENA'S RULES (`k_midair 1` in deathmatch, games/shooters_edge/rules/
// common_midair.cpp over plugins/shooter/midair.hpp): a match of two bots on the baseline arena
// runs to its time limit with no fault, nothing lies on the map, no pack is dropped, every
// kill that scores is a counted midair told to all in its band's words, and a match without the
// setting plays exactly as before (the arena's parity pins, tests/shooters_edge_arena_rules_test).
//
// ge:summary Tests: midair on the arena's rules: a bot match, counted hits only, no items
// ge:keywords test arena native-arena midair k_midair counted-hit band-line mod-match
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/rules/arena.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace sv = shooters_edge::server;

namespace {

sv::ModMatchReport play(const std::vector<std::string>& console) {
  sv::ModMatchOptions o;
  o.native = &sv::arena_rules();
  o.entities = sv::kBaselineArenaEntities;
  o.bots = 2;
  o.max_frames = 5100;
  o.console = console;
  return sv::run_mod_match(o);
}

bool has(const std::string& s, const char* what) { return s.find(what) != std::string::npos; }

}  // namespace

TEST(a_midair_match_runs_to_its_time_limit_without_a_fault) {
  const sv::ModMatchReport r = play({"deathmatch 3", "k_midair 1", "timelimit 1"});
  CHECK_MSG(r.ok, r.error);
  CHECK_MSG(r.faults == 0, r.last_fault);
  CHECK(r.ended);
  for (const std::string& p : r.prints) {
    CHECK_MSG(!has(p, "can't be played"), "the arena refused midair: " + p);
    CHECK_MSG(!has(p, "You got") && !has(p, "You receive"), "an item on a midair map: " + p);
  }
}

TEST(a_midair_kill_is_told_in_its_bands_words) {
  const sv::ModMatchReport r = play({"deathmatch 3", "k_midair 1", "timelimit 1"});
  CHECK_MSG(r.ok, r.error);
  int lines = 0;
  for (const std::string& p : r.prints)
    if (has(p, "midair by ")) {
      lines++;
      CHECK_MSG(has(p, "Bronze") || has(p, "Silver") || has(p, "Gold") || has(p, "Platinum"),
                "a midair line without its band: " + p);
    }
  CHECK_MSG(lines > 0, "no midair hit in a minute of two bots with rockets");
  CHECK_MSG(static_cast<size_t>(lines) <= r.frag_log.size() + 1,
            "more midair lines than kills: " + std::to_string(lines));
}

TEST(the_setting_off_prints_no_midair_line) {
  const sv::ModMatchReport r = play({"deathmatch 3", "timelimit 1"});
  CHECK_MSG(r.ok, r.error);
  for (const std::string& p : r.prints) CHECK_MSG(!has(p, "midair by "), p);
}
