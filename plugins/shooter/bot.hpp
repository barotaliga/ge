// THE BOT: a pilot (pilot.hpp) that sees, hears and looks (senses.hpp, aim.hpp), filling a server's
// client slot. Each command, in order:
// 1. what its client was sent this frame (`client_view`), and the senses taken from it, its view
//    as it last pressed it;
// 2. the pilot's command for the route (it reads only its own player and the movers, as before);
// 3. the view: with a contact seen, or lost under the profile's `hold_s` ago, the yaw and pitch
//    at its predicted middle through the skill's hands (the pilot does not strafe jump while the
//    view is held: the strafe needs the view along the velocity); with none, the view turns back
//    to the pilot's yaw and is handed back within `kRelease` degrees;
// 4. the pilot's forward and side turned under that view (`turn_axes`), so the route is run the
//    same whichever way it looks; the jump and every other button as the pilot pressed them.
// Every decision is on the `ai::Why` (the pilot's `link`, the senses', and `aim`), and the aim's
// intent beside what it pressed (`record_intent`: aim_yaw, aim_pitch).
//
// It fires nothing itself: a game's fighter chooses the weapon and the trigger, and sets how the
// aim leads for it (`shot_speed`: a projectile's speed, the time it flies added to the lead; 0 a
// hitscan; `aim_dz`: aimed this far over the middle, below it for a blast at the feet).
//
// ge:summary The bot: the pilot with senses and aim, a server slot's command source
// ge:keywords bot pilot senses aim view look contact track hold release bot_source
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#pragma once

#include <cstdint>

#include "shooter/aim.hpp"
#include "shooter/pilot.hpp"
#include "shooter/senses.hpp"

namespace ge::shooter {

struct BotStats {
  int64_t commands = 0, aiming = 0, holding = 0;
};

struct Bot {
  PilotBot pilot;
  Senses senses;
  ClientView view;
  AimProfile aim;
  ai::Hands aim_hands;
  bool aiming = false;       // the view is the aim's, not the pilot's
  bool strafe = true;        // the pilot's own strafe setting, put back when the view is let go
  bool hop = false;          // and its hop setting, the same
  int32_t target = -1;       // the contact's entity the view is on
  float yaw = 0, pitch = 0;  // the view last pressed, degrees (yaw unwrapped)
  double wanted[2] = {}, pressed[2] = {};  // the aim's last yaw and pitch, wanted and pressed
  float shot_speed = 0;                    // the held weapon's projectile, units/s; 0 a hitscan
  float aim_dz = 0;                        // the aim point over the target's middle, units
  BotStats stats;
};

// A bot over `g` with the pilot's `hands` and the aim of `skill`, at `hz` commands a second, its
// hands' wander off `seed`. Goals go in `b.pilot.goals`, as for a PilotBot.
void bot_init(Bot& b, const NavGraph& g, const PilotSettings& set, PilotHands hands, AimSkill skill,
              double hz, uint32_t seed);
CmdSource bot_source(Bot& b);
// Sends the bot's pilot to node `goal` alone from its next command (the goals list replaced).
void bot_goal(Bot& b, int32_t goal);
// How far the view is off the aim's wanted angles, degrees (0 while not aiming).
float bot_aim_off(const Bot& b);

}  // namespace ge::shooter
