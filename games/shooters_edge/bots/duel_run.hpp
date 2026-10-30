// A WATCHED RUN on the server's arena, for the senses' test and the `se-senses` lab: a SEER (the
// bot, plugins/shooter/bot.hpp, with an aim skill) and a RUNNER (a plain pilot) fill two bot
// slots and each visits the arena's goals in turn, the runner starting from the east room's. Each
// frame keeps where both stand, the seer's view, whether its senses see the runner, and the
// truth beside it, for measuring only: whether a clear line joins their eyes at all (any facing),
// and the angle between the seer's view and the runner's middle while it is seen.
//
// ge:summary The watched run: a seeing, aiming bot and a runner on the arena, with the truth kept
// ge:keywords duel seer runner senses aim skill arena aim-error line-of-sight reaction
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/bot.hpp"
#include "shooters_edge/server/host.hpp"

namespace shooters_edge::bots {

namespace sh = ge::shooter;

struct Duel {
  server::Host host;
  sh::NavGraph graph;
  sh::Bot seer;
  sh::PilotBot runner;
  int seer_slot = -1, runner_slot = -1;
};

bool make_duel(Duel& d, sh::AimSkill skill, uint32_t seed = 1);

struct DuelResult {
  int frames = 0;
  int64_t line_frames = 0;        // frames a clear line joined the two eyes (whatever the facing)
  int64_t seen_frames = 0;        // frames the seer's senses saw the runner
  int64_t heard = 0;              // sounds the seer heard from the other players
  std::vector<float> error_deg;   // the view off the runner's middle, each frame it was seen
  std::vector<float> reaction_s;  // from acquiring to the view within 5 degrees, each time
  std::vector<sh::Vec3f> seer, runner;  // where each stood, a frame a point
  std::vector<float> view_yaw;          // the seer's view, a frame a value
  std::vector<uint8_t> seen;            // 1 when seen that frame
  std::vector<double> yaw, yaw_wanted;  // the aim's yaw pressed and wanted, while aiming
  sh::SenseStats senses;
  sh::BotStats bot;
};

DuelResult run_duel(Duel& d, int frames);

}  // namespace shooters_edge::bots
