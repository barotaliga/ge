// QUAKE'S OWN DEATHMATCH RULES ON THE ARENA (`localinfo rules q1`, games/shooters_edge/rules/
// common_quake.cpp): a bot match under Quake's rules runs to its limit in every deathmatch mode
// with no fault, the intermission holds the map until a button, the obituaries and the frag log
// keep pace, and the switch off leaves the arena's own match as it was (the parity pins in
// tests/shooters_edge_arena_rules_test.cpp).
//
// ge:summary Tests: Quake's deathmatch rules on the arena: every mode, the intermission, kills
// ge:keywords test arena native-arena quake-rules rules-q1 deathmatch intermission mod-match
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/rules/arena.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace sv = shooters_edge::server;

namespace {

sv::ModMatchReport play(std::vector<std::string> console, int frames = 6000) {
  sv::ModMatchOptions o;
  o.native = &sv::arena_rules();
  o.entities = sv::kBaselineArenaEntities;
  o.press_when_dead = true;
  o.bots = 4;
  o.max_frames = frames;
  o.console = std::move(console);
  return sv::run_mod_match(o);
}

bool has(const std::string& s, const char* what) { return s.find(what) != std::string::npos; }

}  // namespace

TEST(every_deathmatch_mode_runs_under_quakes_rules_without_a_fault) {
  for (const char* dm :
       {"deathmatch 1", "deathmatch 2", "deathmatch 3", "deathmatch 4", "deathmatch 5"}) {
    const sv::ModMatchReport r = play({"localinfo rules q1", dm, "timelimit 1"});
    CHECK_MSG(r.ok, r.error);
    CHECK_MSG(r.faults == 0, std::string(dm) + ": " + r.last_fault);
    CHECK_MSG(r.ended, std::string(dm) + ": the match never moved on to the next map");
  }
}

TEST(the_intermission_waits_five_seconds_then_a_button) {
  const sv::ModMatchReport r = play({"localinfo rules q1", "deathmatch 1", "timelimit 1"});
  CHECK_MSG(r.ok, r.error);
  CHECK(r.ended);
  // The map ends 59 s in (the clock starts at 1 s); the change waits at least 5 s more.
  CHECK_MSG(r.end_time >= 64, "left the intermission early: " + std::to_string(r.end_time));
}

TEST(kills_are_told_and_logged_under_quakes_rules) {
  const sv::ModMatchReport r = play({"localinfo rules q1", "deathmatch 1", "timelimit 1"});
  CHECK_MSG(r.ok, r.error);
  CHECK_MSG(!r.frag_log.empty(), "no frag in a minute of four bots");
  int deaths = 0;
  for (const sv::PlayerPath& p : r.players) deaths += p.deaths;
  // A dead bot presses after letting go, as Quake's rules ask: so the dead stand again.
  CHECK_MSG(deaths >= 3, "too few stood up again after dying: " + std::to_string(deaths));
  for (const std::string& p : r.prints) CHECK_MSG(!has(p, "exited the level"), p);
}
