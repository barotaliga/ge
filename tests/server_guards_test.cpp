// engine/server/guards: the chat flood rule. "N S T" is read and refused when malformed; a fresh
// player has N lines in hand; the (N+1)-th inside S seconds trips a silence of T seconds; the
// window slides; talking while silenced neither counts nor extends the silence; the words a
// player is told.
//
// ge:summary Test: a server's chat flood rule: the parse, the sliding window, the silence
// ge:keywords test flood-protection sv_floodprot flood_check flood_text parse_flood_rule
// ge:skill net testing
#include <string>

#include "check.hpp"
#include "server/guards.hpp"

using namespace ge::server;

TEST(the_rule_is_read_and_a_bad_one_refused) {
  FloodRule r;
  std::string error;
  CHECK(parse_flood_rule("4 4 10", r, error));
  CHECK(r.lines == 4 && r.window_s == 4 && r.silence_s == 10);
  CHECK(flood_rule_text(r) == "4 4 10");
  CHECK(parse_flood_rule("  10 1 1 ", r, error));
  CHECK(flood_rule_text(r) == "10 1 1");
  for (const char* bad :
       {"", "10 1", "11 1 1", "0 1 1", "3 0 1", "3 1 0", "3 1 x", "3 1 1 1", "3.5 1 1"}) {
    error.clear();
    CHECK(!parse_flood_rule(bad, r, error));
    CHECK(error.find("sv_floodprot") != std::string::npos);
    CHECK(error.find("next:") != std::string::npos);
  }
  CHECK(flood_rule_text(r) == "10 1 1");  // a refused text leaves the rule
}

TEST(the_line_past_the_rule_trips_a_silence) {
  FloodRule r;
  r.lines = 3;
  r.window_s = 2;
  r.silence_s = 5;
  FloodGuard g;
  CHECK(flood_check(g, r, 10.0) == FloodVerdict::allowed);
  CHECK(flood_check(g, r, 10.1) == FloodVerdict::allowed);
  CHECK(flood_check(g, r, 10.2) == FloodVerdict::allowed);
  CHECK(flood_check(g, r, 10.3) == FloodVerdict::tripped);
  CHECK(flood_text(FloodVerdict::tripped, g, r, "", 10.3) ==
        "FloodProt: You can't talk for 5 seconds.\n");
  CHECK(flood_text(FloodVerdict::tripped, g, r, "quiet!", 10.3) == "FloodProt: quiet!\n");
  // Silenced until 15.3: lines are dropped and do not extend it.
  CHECK(flood_check(g, r, 12.0) == FloodVerdict::silenced);
  CHECK(flood_text(FloodVerdict::silenced, g, r, "", 12.0) ==
        "You can't talk for 3 more seconds\n");
  CHECK(flood_check(g, r, 15.2) == FloodVerdict::silenced);
  CHECK(g.silent_until == 10.3 + 5);
  CHECK(flood_text(FloodVerdict::allowed, g, r, "", 16) == "");
  // After it, the old lines are outside the window: talking is allowed again.
  CHECK(flood_check(g, r, 15.4) == FloodVerdict::allowed);
}

TEST(the_window_slides) {
  FloodRule r;
  r.lines = 2;
  r.window_s = 1;
  r.silence_s = 1;
  FloodGuard g;
  // A line every 0.6 s never trips two-a-second: each is checked against the one two back.
  for (int i = 0; i < 20; i++) CHECK(flood_check(g, r, 0.6 * i) == FloodVerdict::allowed);
  // Two quick lines after a pause are fine, a third within the second is not.
  FloodGuard h;
  CHECK(flood_check(h, r, 100.0) == FloodVerdict::allowed);
  CHECK(flood_check(h, r, 100.5) == FloodVerdict::allowed);
  CHECK(flood_check(h, r, 100.9) == FloodVerdict::tripped);
}

TEST(the_most_lines_the_ring_holds) {
  FloodRule r;
  r.lines = kMostFloodLines;
  r.window_s = 1;
  r.silence_s = 1;
  FloodGuard g;
  for (int i = 0; i < kMostFloodLines; i++)
    CHECK(flood_check(g, r, 1.0 + 0.01 * i) == FloodVerdict::allowed);
  CHECK(flood_check(g, r, 1.5) == FloodVerdict::tripped);
}
