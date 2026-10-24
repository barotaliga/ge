// The live recorder and the instant replay (engine/replay/live.hpp) over the walker toy game: the
// live run's log is the straight run's; marks keep step order, back-dated or not; an instant
// replay is sought off the live keeper (a clone of the run as opened, never an open) and lands on
// the straight run; the live run is untouched by watching and goes on to the same digest; the
// keeper comes back; a file of the live run plays on the spec.
//
// ge:summary Tests: the instant replay off the live keeper lands on the run; marks; the live file
// ge:keywords test replay live instant-replay watch resume mark back-date keeper file
// ge:skill replay testing
// ge:test tests/replay_live_test.cpp
#include <memory>
#include <span>
#include <string>

#include "check.hpp"
#include "replay/live.hpp"
#include "replay_walker.hpp"

namespace gp = ge::play;
namespace gr = ge::replay;

using namespace replay_test;

namespace {

gp::Setup walker_setup() {
  gp::Setup setup;
  std::string error;
  gp::make_setup(kWalker, 7, {}, setup, error);
  return setup;
}

void step_live(gr::Live& live, int from, int to) {
  for (int i = from; i < to; i++) {
    const double v = push_at(i);
    live.step(std::span<const double>(&v, 1));
  }
}

}  // namespace

TEST(an_instant_replay_lands_on_the_run_and_the_run_goes_on) {
  const gp::InputLog log = walk_log();
  g_opens = 0;
  gr::Live live(kWalker, walker_setup(), kPolicy);
  CHECK_MSG(live.ok(), live.error());
  step_live(live, 0, 200);
  const uint64_t live_at_200 = live.run().digest();
  CHECK(live_at_200 == straight(log, 200));
  CHECK(live.keeper().size() > 3);

  // The last 3.4 s: from the live keeper's copy at 160, six steps; no run opened again.
  std::unique_ptr<gr::Player> replay = live.watch(3.4);
  CHECK(replay && live.watching() && live.keeper().size() == 0);
  CHECK(replay->target() == 166 && replay->length() == 200);
  CHECK(replay->advance(1000));
  CHECK(replay->run().digest() == straight(log, 166));
  CHECK(replay->stats().restores == 1 && replay->stats().steps == 6);
  CHECK(g_opens == 1);
  CHECK(!live.watch(1));  // one replay at a time

  // Back to the run's start and to the end of the log.
  replay->seek(0);
  CHECK(replay->advance(1000) && replay->run().digest() == straight(log, 0));
  replay->seek(200);
  CHECK(replay->advance(1000) && replay->run().digest() == live_at_200);
  CHECK(g_opens == 1);

  live.resume(*replay);
  CHECK(!live.watching() && live.keeper().size() > 3);
  CHECK(live.run().digest() == live_at_200);  // set aside untouched
  step_live(live, 200, 300);
  CHECK(live.run().digest() == straight(log, 300));
  CHECK(live.keeper().newest() == 300);
}

TEST(the_live_run_steps_on_while_a_replay_is_out) {
  const gp::InputLog log = walk_log();
  gr::Live live(kWalker, walker_setup(), kPolicy);
  step_live(live, 0, 100);
  auto replay = live.watch(30);  // further back than the run: its start
  CHECK(replay && replay->target() == 0);
  step_live(live, 100, 150);  // no copies taken while the keeper is out
  CHECK(replay->advance(1000) && replay->run().digest() == straight(log, 0));
  live.resume(*replay);
  CHECK(live.keeper().newest() == 150);  // the copy offered on the way back
  step_live(live, 150, 300);
  CHECK(live.run().digest() == straight(log, 300));
}

TEST(marks_keep_step_order_and_the_file_plays) {
  gr::Live live(kWalker, walker_setup(), kPolicy);
  step_live(live, 0, 50);
  live.mark("finish", 2);
  live.mark_at(12, "takeoff");
  live.mark_at(50, "landing", 0.5);  // after the finish filed at the same step
  live.mark_at(999, "late");         // held at now
  live.mark("Not A Kind");
  CHECK(live.bad_marks() == 1);
  const auto& m = live.marks();
  CHECK(m.size() == 4 && m[0].kind == "takeoff" && m[1].kind == "finish" &&
        m[2].kind == "landing" && m[3].step == 50);
  const gr::ReplayFile f = live.file("today");
  std::string error;
  CHECK(gr::plays_on(f, kWalker, error));
  CHECK(f.log.steps() == 50 && f.log.digest == live.run().digest() && f.recorded == "today");
  CHECK(f.game_version == kWalker.version);
}
