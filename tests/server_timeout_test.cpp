// Tests: timeouts in a match (engine/server/timeout.hpp). A player calls one, the game stays
// stopped for its length and counts back to play; the side that called it may end it sooner and no
// one else may; each side has its timeouts for the map, and gets them back at the next; spectators,
// a warmup and a server with none are refused with the reason.
//
// ge:summary Tests: timeouts called, run out, ended early, counted per side, refused
// ge:keywords test timeout timein sv_timeouts per-side pause
// ge:skill net testing
#include <string>

#include "check.hpp"
#include "server/timeout.hpp"

namespace sv = ge::server;

namespace {

const sv::TimeoutCaller kAnn{0, "Ann", "red", true};
const sv::TimeoutCaller kBo{1, "Bo", "blue", true};
const sv::TimeoutCaller kCy{2, "Cy", "red", true};
const sv::TimeoutCaller kWatcher{3, "Dee", "Dee", false};

std::string last(const sv::TimeoutBook& b) { return b.said.empty() ? "" : b.said.back(); }

// The clock run on from `from` to `to` in frames of 1/72 s.
void run(sv::TimeoutBook& b, double from, double to) {
  const int frames = static_cast<int>((to - from) * 72);
  for (int f = 0; f <= frames; f++) sv::timeout_tick(b, from + f / 72.0);
}

}  // namespace

TEST(a_timeout_stops_the_game_for_its_length_then_counts_back_to_play) {
  sv::TimeoutBook b;
  CHECK(!sv::timeout_command(b, kAnn, true, "say hi", 0));
  CHECK(sv::timeout_command(b, kAnn, true, "timeout", 100));
  CHECK(sv::timeout_paused(b) && b.phase == sv::TimeoutPhase::out && b.to.back() == -1);
  CHECK_MSG(last(b) == "Ann calls a timeout. The game stops for 60 seconds.", last(b));
  b.said.clear();
  b.to.clear();
  run(b, 100, 159.9);
  CHECK(b.phase == sv::TimeoutPhase::out);
  CHECK(b.said.size() == 2 && b.said[0] == "30 seconds left in the timeout." &&
        b.said[1] == "10 seconds left in the timeout.");
  run(b, 160, 164.9);
  CHECK(b.phase == sv::TimeoutPhase::resuming && sv::timeout_paused(b));
  CHECK_MSG(b.said[2] == "The timeout is over. Play resumes in 5 seconds.", b.said[2]);
  CHECK(b.said.size() == 6 && b.said[3] == "3..." && b.said[5] == "1...");
  run(b, 165, 165.1);
  CHECK(!sv::timeout_paused(b) && last(b) == "Play!");
}

TEST(only_the_side_that_called_it_ends_a_timeout_early) {
  sv::TimeoutBook b;
  b.settings.resume_s = 3;
  sv::timeout_command(b, kAnn, true, "timeout", 0);
  sv::timeout_command(b, kBo, true, "timein", 5);
  CHECK(b.phase == sv::TimeoutPhase::out && b.to.back() == 1);
  CHECK_MSG(last(b) == "Only Ann's side can end this timeout early.", last(b));
  sv::timeout_command(b, kBo, true, "timeout", 5.5);
  CHECK_MSG(
      last(b) == "Ann's timeout: 55 seconds left. Type timein to resume sooner if it is yours.",
      last(b));
  sv::timeout_command(b, kCy, true, "timein", 6);  // Ann's teammate
  CHECK(b.phase == sv::TimeoutPhase::resuming);
  CHECK(b.said[b.said.size() - 2] == "Cy calls time in.");
  run(b, 6, 9.1);
  CHECK(!sv::timeout_paused(b) && last(b) == "Play!");
}

TEST(each_side_has_its_timeouts_for_the_map) {
  sv::TimeoutBook b;
  b.settings.length_s = 10;
  b.settings.resume_s = 0;
  sv::timeout_command(b, kAnn, true, "timeout", 0);
  run(b, 0, 10.1);
  CHECK(!sv::timeout_paused(b) && sv::timeout_left(b, "red") == 0);
  sv::timeout_command(b, kCy, true, "timeout", 20);  // red's is used
  CHECK(!sv::timeout_paused(b) && b.to.back() == 2);
  CHECK_MSG(last(b) == "Your side has used its timeout for this map.", last(b));
  CHECK(sv::timeout_left(b, "blue") == 1);
  sv::timeout_command(b, kBo, true, "timeout", 21);
  CHECK(sv::timeout_paused(b) && b.called == 2);
  sv::timeout_map_over(b);
  CHECK(!sv::timeout_paused(b) && sv::timeout_left(b, "red") == 1);
}

TEST(a_timeout_is_refused_off_the_match_to_watchers_and_where_there_are_none) {
  sv::TimeoutBook b;
  sv::timeout_command(b, kWatcher, true, "timeout", 0);
  CHECK(!sv::timeout_paused(b) && last(b) == "Only a player in the match can call a timeout.");
  sv::timeout_command(b, kAnn, false, "timeout", 0);
  CHECK(!sv::timeout_paused(b) && last(b) == "A timeout can be called only while the match is on.");
  sv::timeout_command(b, kAnn, true, "timein", 0);
  CHECK(last(b) == "There is no timeout to end.");
  b.settings.per_side = 0;
  sv::timeout_command(b, kAnn, true, "timeout", 0);
  CHECK(!sv::timeout_paused(b) && last(b) == "Timeouts are off on this server.");
  CHECK(b.called == 0);
}
