// THE BOT: a pilot (pilot.hpp) that sees, hears and looks (senses.hpp, aim.hpp), filling a server's
// client slot. Each command, in order:
// 1. what its client was sent this frame (`client_view`), and the senses taken from it, its view
//    as it last pressed it;
// 2. the pilot's command for the route (it reads only its own player and the movers, as before);
// 3. the view: with a contact seen, or lost under the profile's `hold_s` ago (the freshest; the
//    one in hand kept while in sight unless another is half as far), the yaw and pitch
//    at its predicted middle through the skill's hands, a new contact first waiting a reaction
//    drawn for it (half of it when the view was already on him: pre-aimed) (the pilot does not
//    strafe jump while the view is held: the strafe needs the view along the velocity); with none,
//    the view turns back to the pilot's yaw (a calm view at a hand's pace, `kBackTurn`) and is
//    handed back within `kRelease` degrees (a calm walker's within the calm band), unless
//    the game set a spot to watch (`look`, `look_at`: a doorway an enemy comes in by), where it
//    goes on the same hands, so a contact showing there is pre-aimed at;
// 4. the pilot's forward and side turned under that view (`turn_axes`), so the route is run the
//    same whichever way it looks; the jump and every other button as the pilot pressed them.
// Every decision is on the `ai::Why` (the pilot's `link`, the senses', and `aim`), and the aim's
// intent beside what it pressed (`record_intent`: aim_yaw, aim_pitch).
//
// It fires nothing itself: a game's fighter chooses the weapon and the trigger, and sets how the
// aim leads for it (`shot_speed`: a projectile's speed, the time it flies added to the lead; 0 a
// hitscan; `aim_dz`: aimed this far over the middle, below it for a blast at the feet).
//
// THE INTERCEPT (`intercepts`, `target_arc`, `lob`; shooter/intercept.hpp): a projectile is fired
// at where its target will be when it gets there, the flight worked out again from that place;
// a target in the air is led along his own arc (where he comes down is where a midair shot goes,
// a hitscan's too); a grenade's pitch is its arc's, from the origin it leaves at.
//
// THE NETWORK'S LEAD (`lag_s`, `rewound`): on a server across a network, the others are seen
// where the server last sent them and the shot starts there a one-way trip after it is pressed,
// so the target has gone on a round trip: a projectile is led `lag_s` more, and a hitscan too
// unless the server rewinds hitscan to what the shooter saw (`rewound`, its `sv_antilag`) -- but
// only while the target is in the air, on his own arc: on the ground a strafer's steps change
// faster than a round trip, and that lead on top of the hands' own swings the aim wider than it
// gains (measured: se-team at 200 ms keeps a strong aim at its LAN rate with none there).
//
// ge:summary The bot: the pilot with senses and aim, a server slot's command source
// ge:keywords bot pilot senses aim view look contact track hold release bot_source
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#pragma once

#include <cstdint>

#include "core/prng.hpp"
#include "shooter/aim.hpp"
#include "shooter/pilot.hpp"
#include "shooter/senses.hpp"

namespace ge::shooter {

struct BotStats {
  int64_t commands = 0, aiming = 0, holding = 0;
  int64_t looking = 0;                   // commands the view spent on a spot watched (`look`)
  int64_t reactions = 0, pre_aimed = 0;  // sightings reacted to, and of them, ones pre-aimed at
  double react_s = 0;                    // the reaction times drawn, summed
  int64_t kept = 0;  // commands the one in hand was kept over a nearer one in sight
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
  // How the aim meets a projectile's target (intercept.hpp): the flight worked out this many
  // times (1: the plain distance over the speed), the target's own arc when he is in the air, and
  // the held weapon thrown as a grenade is (its pitch off the arc).
  // THE CALM VIEW (`calm`): walking, waiting or riding with nobody to aim at, the view is a hand
  // on a mouse, not the route's heading: it looks at the route's point a second or so on (turning
  // before a corner, to see round it), still within `kCalmBand` of that (the keys
  // turned under it, so the walk is the same), turned at a hand's pace past it (`kCalmTurn`, an
  // about-face `kCalmAbout`), with a slow drift of a degree or two. A strafe jump, a hop or a
  // jump keeps the pilot's own view: their air control is the view's.
  bool calm = false;
  double calm_t = 0;  // the drift's clock, s
  int intercepts = 1;
  bool target_arc = false;
  bool lob = false;
  float lag_s = 0;       // the round trip to the server, s (0: the bot's slot on the server)
  bool rewound = false;  // the server rewinds hitscan to what the shooter saw (sv_antilag)
  // The reaction to the sighting in hand (aim.hpp `draw_reaction`): the view held where it was
  // until `freeze_until` (the hands' own lag then makes up the rest), and no shot before
  // `react_until`.
  double freeze_until = -1e9, react_until = -1e9;
  double target_seen_at = -1e9;  // when the one in hand was last in sight
  double freeze_view[2] = {};
  // A spot watched with no contact in hand (a doorway an enemy comes in by): while `look` is set
  // the view goes to `look_at` on the aim's hands, pre-aimed there, and when it is let go, back
  // onto the pilot's way the same way. `looking`: the view is the look's (or on its way back).
  bool look = false;
  bool looking = false;
  Vec3f look_at;
  core::Rng react_rng{1};  // the reactions' draws, off the bot's seed
  BotStats stats;
};

// A bot over `g` with the pilot's `hands` and the aim of `skill`, at `hz` commands a second, its
// hands' wander off `seed`. Goals go in `b.pilot.goals`, as for a PilotBot.
void bot_init(Bot& b, const NavGraph& g, const PilotSettings& set, PilotHands hands, AimSkill skill,
              double hz, uint32_t seed);
// The same with the aim's hands given (traits.hpp's per weapon and reaction).
void bot_init(Bot& b, const NavGraph& g, const PilotSettings& set, PilotHands hands,
              const AimProfile& aim, double hz, uint32_t seed);
CmdSource bot_source(Bot& b);
// Sends the bot's pilot to node `goal` alone from its next command (the goals list replaced).
void bot_goal(Bot& b, int32_t goal);
// The aim's hands from now on as `aim` holds them (another weapon taken up), kept where they are.
void bot_aim(Bot& b, const AimProfile& aim);
// How far the view is off the aim's wanted angles, degrees (0 while not aiming).
float bot_aim_off(const Bot& b);
// Whether the bot is still reacting to the contact it took up at server time `t` (no shot yet).
bool bot_reacting(const Bot& b, double t);

}  // namespace ge::shooter
