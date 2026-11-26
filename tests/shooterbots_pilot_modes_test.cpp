// The pilot's modes as the bot reads them (plugins/shooterbots/pilot.hpp): a jump lined up, run at
// and flown is the pilot's own view, so a spot the bot watches waits meanwhile (e1m4's trap room:
// a button watched across the slime pit held the view off the jump across it for 300 s).
//
// ge:summary Tests for the pilot's modes that keep the view: a jump's run-up, run and flight
// ge:keywords test pilot mode leaping_mode run-up jump air look watch view button-poke
// ge:skill shooterbots testing
#include "check.hpp"
#include "shooterbots/pilot.hpp"

namespace {

namespace sb = ge::shooterbots;

TEST(a_jump_lined_up_run_at_and_flown_keeps_the_view) {
  CHECK(sb::leaping_mode(sb::PilotMode::run_up));
  CHECK(sb::leaping_mode(sb::PilotMode::jump));
  CHECK(sb::leaping_mode(sb::PilotMode::air));
}

TEST(a_walk_a_wait_and_a_swim_let_a_watched_spot_have_it) {
  for (const sb::PilotMode m : {sb::PilotMode::idle, sb::PilotMode::walk, sb::PilotMode::wait,
                                sb::PilotMode::ride, sb::PilotMode::strafe, sb::PilotMode::arrived,
                                sb::PilotMode::hop, sb::PilotMode::swim, sb::PilotMode::grenade})
    CHECK(!sb::leaping_mode(m));
}

}  // namespace
