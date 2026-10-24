// The replay's player (engine/replay/player.hpp) over a toy game whose state is a stream of
// draws: a seek to any step, back or forward, in any budget, has the digest and the state of the
// run played straight there; a seek behind the first copy reopens from the setup; a seek ahead
// past a copy starts from it; a log of another game is refused with what differs.
//
// ge:summary Tests: the replay player's seek lands on the run played straight there
// ge:keywords test replay player seek advance budget restore reopen digest
// ge:skill replay testing
// ge:test tests/replay_player_test.cpp
#include <string>
#include <vector>

#include "check.hpp"
#include "play/inputs.hpp"
#include "replay/player.hpp"
#include "replay_walker.hpp"

namespace gp = ge::play;
namespace gr = ge::replay;

using namespace replay_test;

TEST(every_seek_lands_on_the_straight_run) {
  const gp::InputLog log = walk_log();
  gr::Player p(kWalker, log, kPolicy);
  CHECK(p.ok());
  CHECK(p.length() == 300);
  const std::vector<int64_t> marks{300, 17, 250, 251, 0, 299, 3, 120, 80, 300, 45};
  for (int64_t m : marks) {
    p.seek(m);
    while (!p.advance(13)) {
    }
    CHECK_MSG(p.at() == m && p.run().digest() == straight(log, m),
              "seek to " + std::to_string(m) + " parted from the straight run");
  }
  CHECK(p.stats().restores > 0);
}

TEST(where_a_seek_starts) {
  const gp::InputLog log = walk_log();
  gr::Player p(kWalker, log, kPolicy);
  p.seek(300);
  CHECK(p.advance(1000));
  CHECK(p.stats().steps == 300 && p.stats().restores == 0);
  // At step 300 the copies are 0, 40 .. 160, 200 .. 240 by 20 and 250 .. 300 by 10: back to 287
  // is the copy at 280 and seven steps.
  p.seek(287);
  CHECK(p.advance(1000));
  CHECK(p.stats().restores == 1);
  CHECK(p.stats().steps == 307);
  // Behind every copy but the first: from the copy of the run as opened.
  p.seek(2);
  CHECK(p.advance(1000) && p.run().digest() == straight(log, 2));
}

TEST(a_budget_pays_a_long_seek_out) {
  const gp::InputLog log = walk_log();
  gr::Player p(kWalker, log, kPolicy);
  p.seek(200);
  int frames = 0;
  while (!p.advance(25)) frames++;
  CHECK(frames == 7);  // 200 steps at 25 a frame: seven short frames, then the one that lands
  CHECK(p.run().digest() == straight(log, 200));
}

TEST(a_log_of_another_game_is_refused) {
  gp::InputLog log = walk_log();
  log.game = "other";
  gr::Player p(kWalker, log, kPolicy);
  CHECK(!p.ok() && !p.error().empty());
  gr::Player bad(kWalker, walk_log(), {0.15, 5, 3, 10, 6, 0});  // 1.5 steps
  CHECK(!bad.ok() && bad.error().find("keep policy") != std::string::npos);
}
