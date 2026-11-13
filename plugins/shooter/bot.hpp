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
//    handed back within `kRelease` degrees (a calm walker's within the calm band, a glance aside
//    now and then: `glance`), unless
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
// ge:keywords lookway calm-view
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
  int64_t looking = 0;      // commands the view spent on a spot watched (`look`)
  int64_t aim_hop_air = 0;  // commands in a hop's air under the aim's view (`aimhop`)
  int64_t air_keys = 0;     // and under the hand's view (`airkeys`)
  int64_t circle_keys = 0;  // a circle jump's commands on the ground pressed by `circlekeys`
  int64_t reactions = 0, pre_aimed = 0;  // sightings reacted to, and of them, ones pre-aimed at
  double react_s = 0;                    // the reaction times drawn, summed
  int64_t kept = 0;          // commands the one in hand was kept over a nearer one in sight
  int64_t sound_swipes = 0;  // swipes of the view at a sound (`listen`)
};

struct Bot {
  PilotBot pilot;
  Senses senses;
  ClientView view;
  AimProfile aim;
  ai::Hands aim_hands;
  bool aiming = false;  // the view is the aim's, not the pilot's
  bool strafe = true;   // the pilot's own strafe setting, put back when the view is let go
  bool hop = false;     // and its hop setting, the same
  // THE HOP WHILE AIMING (`aimhop`): the pilot's hop is not turned off by the aim; its landings are
  // jumped, and in the air the key set under the aim's view (eight and none) that carries the
  // velocity furthest along the hop's way is pressed (`aim_air_keys`).
  bool aimhop = false;
  int air_set = -1;  // and the set pressed in the air last command (-1 none)
  // A HOP'S AIR UNDER THE HAND (`airkeys`): the pilot's side key (alone or with forward) pressed
  // only while it carries the velocity on under the view the hand has pressed.
  bool airkeys = false;
  bool airview = false;  // and the hand's view on the velocity in a hop's air (`airview`)
  bool airwall = false;  // and the air's keys charged for a wall ahead (`airwall`)
  // A WALK IN THE AIR (`airwalk`): the air's key set toward the walk's target under the view
  // pressed, not the walk's forward key; and the set pressed last command (-1 none).
  bool airwalk = false;
  // and each air set flown held through the wall's look, scored by the speed it keeps (`airarc`).
  bool airarc = false;
  // NO FORWARD IN A HOP (`hopside`): the air's sets the strafe key alone, the side the view turns.
  bool hopside = false;
  // ROAMING, NO VIEW BEHIND (`noback`): set by the game while it roams; a player newly seen off
  // behind the way walked is not turned to.
  bool aim_front_only = false;
  // A LOST ONE BEHIND LET GO (`letgo`): the aim's hold on
  // a player out of sight (where he was, carried along) is let go once he lies off behind the way
  // walked, rather than walked away from backwards.
  bool hold_front_only = false;
  // BACKING OFF, NO AIM BEHIND BUT AT A CHASER (`retreatrun`): set by the game while it backs off;
  // one off behind the way walked who is not coming on is not aimed at.
  bool run_front_only = false;
  // THE VIEW BACK TO LEVEL (`levelview`): a look let go comes back level under the hand.
  bool levelview = false;
  // THE VIEW BACK BY THE HAND'S OWN STROKE (`paceback`): under the mouse hand, an aim lost or a
  // look let go goes back to the way by the hand's stroke onto it, not a want paced from the view.
  bool paceback = false;
  int walk_air_set = -1;
  // A CIRCLE JUMP'S CURVE UNDER THE HAND (`circlekeys`): on the ground, the key set the ground move
  // carries furthest on round the way under the view the keys land under, as the air's above.
  bool circlekeys = false;
  // and under the mouse hand as people's (`circleview`): the hand on the velocity's heading,
  // forward and the curve's side key held: the wish 45 degrees off the velocity gains in a wide
  // window, and a hand behind the turn moves it toward the best angle, not off the gain.
  bool circleview = false;
  // and only under the hand's own view (`circleaim`): none while aiming (the hop's first jump at
  // once), a holder's spot watched given up while one curves (watch.cpp).
  bool circleaim = false;
  // THE LEAD ALONG THE WALLS (`leadwall`): a player's predicted way slid along the walls it meets
  // (intercept.hpp `MovingTarget::slide`), not run on through them.
  bool leadwall = false;
  // THE DODGER'S MIDDLE (`leadmean`): a projectile led on the target's drift (senses.hpp
  // `Contact::drift`) as its flight lengthens, not on his course of the moment.
  bool leadmean = false;
  // THE HOPPER'S CURVE (`leadarc`): a projectile led along the target's arc at his turn rate
  // (senses.hpp `Contact::turn`), not a straight line.
  bool leadarc = false;
  int32_t target = -1;                     // the contact's entity the view is on
  float yaw = 0, pitch = 0;                // the view last pressed, degrees (yaw unwrapped)
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
  // Switches (features.hpp): the one in hand kept over a nearer one unless that one is half as far
  // (`keep`; off, the freshest is taken every command), and a calm view turned back at a hand's
  // pace (`paced`; off, the aim's flick).
  bool keep = true;
  bool paced = true;
  bool trial = false;  // the code under an A/B reads it (features.hpp `trial`)
  // THE OWN MOTION (`selfaim`): the view is worked out from where its eye will be `aim.lead_s`
  // on, along its own velocity, as a person's hand allows for his own strafe; off, the hands
  // chase the turn its own motion makes. Measured on e1m2 (se-team, both sides reading a course
  // change a reaction late): accuracy 31.7 -> 35.9 %, red with it against blue without 36.3 to
  // 30.4 %, and the dodge no longer costs the dodger its aim.
  bool selfaim = true;
  // THE CORNER (`corner`): an enemy lost from sight (behind a wall, under water) is carried along
  // his course only `kLostCarry_s` (bot.cpp: the moment it takes to see him gone), then the view
  // holds where he went, as a person's crosshair waits at the corner; off, his spot is carried
  // along for the whole `aim.hold_s` and the view tracks him through the wall.
  bool corner = true;
  // THE MOUSE HAND (`mouse`, ai/mouse.hpp, with the aim profile's `mouse` set): the aim's and the
  // calm view's turns are a hand's strokes on a pad (a bell of speed, Fitts' time, landing a
  // little off, lifts, a drag right running down), not a spring or a steady rate; a turn meant
  // (onto an enemy) at full pace, the calm view's at `kCalmPace` of it. With a shot weapon the
  // hand does not trace the enemy between shots: its stroke onto him is timed to land as the
  // weapon comes ready (`shot_due_s`, from the fighter); a beam (`beam`) is traced. The finger
  // clicks fire (`finger`, the fighter's) rather than holding it.
  bool mouse = true;
  ai::Mouse hand;
  ai::Finger finger;
  double shot_due_s = 0;  // the weapon ready again in this long, s (0: ready)
  bool beam = false;      // the weapon in hand fires a stream: traced, and fire held
  float aim_dist = 0;     // the contact aimed at this far, units (the stroke's target width)
  // What turned the view this command ("aim", "look", "calm", "air", "swim", "pilot"): read by
  // se-team's trace.
  const char* view_by = "pilot";
  // For the trace alone (nothing reads them): the heading the pilot's wish runs along and the
  // calm view's target this command, degrees.
  float walk_yaw = 0, look_yaw = 0;
  // Which of the game's views was set on the command after the step (`bot_hand_override`), for
  // the trace: "button", "swimway", "swimexit", ...; null: "game".
  const char* set_by = nullptr;
  // Swimming, the pilot's wish pressed under the aim's or the calm view in three dimensions (its
  // dive and rise kept); off, turned in the plane alone (features.hpp `pools`).
  bool pools = true;
  // THE GLANCE (`glance`): a calm walker looks aside every `kGlanceMin_s` to that plus
  // `kGlanceSpread_s` (drawn), 75 or 120 degrees off where it looks, to the side whose line of
  // sight runs longest (past 400 units), held a moment and swiped back:
  // people check the open ground beside them, and a still view misses who comes from there.
  // Measured on e1m2 (se-team, red with it against blue without, twelve seeds): about 65 frags a
  // six-seed set to the side that glances, its view still turning 104 degrees a second (people
  // 107).
  bool glance = true;
  // THE WAY IN SIGHT (`lookway`): the calm view on the route's furthest point up to `kCalmAhead`
  // on that the eye still sees, and never past a teleporter: the point that far along the route
  // lay behind the run round a switchback or across a teleporter (e1m2, calm walkers: the look
  // point over 120 degrees off the run 17 % of their time; people's view within 15 degrees of
  // their run 48 %, the bots' 23 %).
  bool lookway = false;
  // THE RUN'S WAY (`calmvel`): moving past 150 units/s, the calm view looks the way the run goes,
  // not at the route's point `kCalmAhead` on (through a corner's wall): people's view lies within
  // 15 degrees of their run 48 % of a calm run, and enemies appear ahead of it.
  bool calm_vel = false;
  double glance_next = 0, glance_until = -1;
  float glance_off = 0;
  // The glance is a SWIPE (a turn meant's pace there and back), held `kGlanceHold_s` from when
  // it lands (the swipe against the calm view's pace, red alone, six seeds: sighting 57.7 ->
  // 61.6 % of the enemies on its screen, frags +9 a set: neutral).
  bool glance_landed = false;
  double glance_back_until = -1;
  // THE SOUND'S SWIPE (`listen`): a calm bot that hears an enemy where it knew of nobody (not
  // seen within `kSoundKnown_s` near there) swipes its view there at a turn meant's pace, holds it
  // twice its reaction's median (an expert about 0.3 s, a novice 0.6), and swipes back: a person
  // hearing a step round him looks. Not a view fixed on the sound (tried and dropped: it stopped
  // the scan).
  bool listen = true;
  double sound_heard_at = -1;  // the newest sound swiped at (or let be), s
  double sound_until = -1, sound_back_until = -1;
  float sound_yaw = 0;
  bool sound_landed = false;
  core::Rng glance_rng{1};  // the glances' draws, off the bot's seed
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
// The goal `goal` planned, and on through it (the point `at`: an item beside its node) to `then`
// (`pilot_chain`), so a pickup on the way is run through rather than stopped at.
void bot_goal(Bot& b, int32_t goal, int32_t then, Vec3f at);
// The route under way chained on from its goal to `then` (`pilot_chain`); false: unchanged.
bool bot_chain(Bot& b, int32_t then, Vec3f at);
// The aim's hands from now on as `aim` holds them (another weapon taken up), kept where they are.
void bot_aim(Bot& b, const AimProfile& aim);
// How far the view is off the aim's wanted angles, degrees (0 while not aiming).
float bot_aim_off(const Bot& b);
// Whether the bot is still reacting to the contact it took up at server time `t` (no shot yet).
bool bot_reacting(const Bot& b, double t);
// A view the game set on `cmd` after the bot's step (a button aimed at, a doorway fired at, a
// swim's way out) turned to by the mouse hand (`mouse`) instead of jumping there: the hand a
// `msec` step toward it, the wish kept in the world (in three dimensions when `wet`). Returns
// how far the view still lies off the one set, degrees (0: none set, or no hand).
float bot_hand_override(Bot& b, MoveCmd& cmd, int msec, bool wet);

}  // namespace ge::shooter
