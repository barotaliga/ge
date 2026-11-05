// THE CO-OP COMPANION: a fighter (fighter.hpp) that plays the campaign beside a person rather than
// for him. It is an ENABLER: it keeps close, clears what attacks the one it escorts, presses the
// buttons he needs, and leaves the level, its monsters and its secrets to him. It reads only what
// its client is sent (the players' and the monsters' places, models and angles, the sounds, its
// own stats), like every Shooter's Edge bot.
//
// ITS MODES (`EscortMode`), chosen every think (fighter.hpp's `kThink_s`):
// - DEFEND: a monster that THREATENS (below) is in sight: the view on it and the trigger, from
//   where it stands, never further than `kEscortLeash` from the leader; it does not chase it round
//   the map;
// - BUTTON: a button within `kEscortButtonNear` of the leader that he has stood near for
//   `kEscortButtonWait_s` and that is still at rest, with a door it opens still shut: it goes and
//   presses it (walks into it, or shoots it when it opens by a shot), once;
// - CATCH UP: the leader more than `kEscortCatchUp` away: it runs to him, hopping when its moves
// allow;
// - FOLLOW: to a spot at the leader's shoulder (`kEscortNear` from him, behind his view, a side of
// its
//   own) whenever he moves on or it is further than `kEscortFar`;
// - and in every mode it GIVES WAY: ahead of him as he walks and near his line, it steps aside;
// - BORED: the leader still for longer than its patience (`bored_after_s`, off its traits) with
//   nothing in sight: it fidgets as a person waiting does (plugins/shooter/idle.hpp: spins, looks
//   at an item in sight, glances at the leader, hops round him, jumps, sways), and stops within a
//   human moment once he moves on or something shows.
//
// WHAT THREATENS: a monster it can see (its model one of the profile's `kClassicMonsters`, not
// lying dead) that is AWAKE (it moved, turned or made a sound within `kEscortStir_s`: one asleep on
// its spot is left for the leader to find) and either near the leader (`kEscortGuard`) or near the
// companion (`kEscortSelf`), or facing one of them within `kEscortFacing` degrees inside
// `kEscortSight`. Only those are its senses' foes (shooter/senses.hpp `Senses::foes`), so the aim
// never turns to a monster that has not come for them. Every player is a friend: its shots are held
// while one stands in the line (the co-op rules spare teammates under teamplay; its own blast does
// not).
//
// Every choice is on the `ai::Why`, and the counts (`EscortStats`) are what the `se-coop` lab and
// the companion's test read.
//
// Deterministic: the fighter's own stream draws the boredom's acts, the server's state is read as
// a client is sent it.
//
// ge:summary The co-op companion: follows the leader, defends him, presses buttons, idles humanly
// ge:keywords companion co-op coop escort follow leader defend threat monsters button press
// ge:keywords bored idle enabler catch-up shoulder escort_init escort_think escort_command
// ge:skill shooter bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "ai/why.hpp"
#include "shooter/idle.hpp"
#include "shooter/usercmd.hpp"
#include "shooter/vec.hpp"

namespace shooters_edge::server {
struct RulesCommon;
}

namespace shooters_edge::bots {

namespace sh = ge::shooter;

struct Fighter;
struct Stats;

constexpr float kEscortNear = 120;     // the shoulder spot's distance from the leader, units
constexpr float kEscortFar = 220;      // further than this: follow again, units
constexpr float kEscortLeash = 360;    // defending, never further than this from him, units
constexpr float kEscortCatchUp = 720;  // further than this: run (and hop) to catch up, units
constexpr float kEscortGuard = 640;    // a waking monster this near the leader threatens, units
constexpr float kEscortSelf = 420;     // or this near the companion, units
constexpr float kEscortSight = 1400;   // a monster facing them within this threatens, units
constexpr float kEscortFacing = 30;    // facing: its yaw within this of the line to them, degrees
constexpr double kEscortStir_s = 2;  // a monster that moved or made a sound this long ago is awake
constexpr float kEscortButtonNear = 480;     // a button this near the leader is his to need, units
constexpr double kEscortButtonWait_s = 1.2;  // after he has been near it this long, s
constexpr double kEscortButtonGive_s = 8;    // a press not done in this long is given up, s

enum class EscortMode : uint8_t { follow, defend, button, catch_up, bored };
inline constexpr int kEscortModes = 5;
const char* escort_mode_name(EscortMode m);

struct EscortStats {
  int64_t commands[kEscortModes] = {};  // commands spent in each mode
  int64_t threats = 0;                  // monsters taken on (became its foe)
  int64_t buttons = 0;                  // buttons pressed (moved off their rest)
  int64_t buttons_given_up = 0;
  int64_t idle_breaks = 0;  // boredom broken off (the leader moved, or something showed)
  int64_t shots_held = 0;   // shots held for a player in the line
  int64_t gave_way = 0;     // commands spent stepping out of his way
};

struct Escort {
  // Ordered by size, largest first.
  std::vector<uint8_t> monster_model;  // by model index: the profile names it a monster
  std::vector<double> stirred_at;      // by entity: when it last moved, turned or sounded
  std::vector<sh::Vec3f> last_at;      // by entity: where it was at the last command
  std::vector<float> last_yaw;         // and which way it faced, degrees
  std::vector<sh::Vec3f> rest;         // by entity: where it stood when the companion joined
  std::vector<sh::Vec3f> button_rest;  // by the fighter's button spot: its origin at rest
  std::vector<uint8_t> button_done;    // and whether it was pressed (or given up)
  sh::IdleState idle;
  sh::IdleWeights weights;
  EscortStats stats;
  sh::Vec3f leader_at, still_at;  // the leader now, and where he stood still since `still_since`
  sh::Vec3f leader_vel;           // his velocity, off his last two places, units/s
  sh::Vec3f look_at;              // a LOOK's point
  double still_since = 0;
  double bored_after_s = 4;            // the leader still this long: bored (its patience)
  double near_button_since = -1;       // the leader near `button` since then (-1: no candidate)
  double button_since = -1;            // the press under way began then
  double break_at = -1;                // boredom broken: the act stops then (a human moment later)
  double last_t = -1;                  // the server time of the last command
  int leader = -1;                     // the slot it escorts (-1: not a companion)
  int threat = -1;                     // the monster its view is on (-1: none)
  int button = -1;                     // the button spot it wants pressed (-1: none)
  int32_t follow_node = -1;            // the shoulder spot's node
  int32_t hop_node = -1;               // a bored hop's turn point
  int side = 1;                        // its shoulder: -1 left, 1 right
  float view_yaw = 0, view_pitch = 0;  // the view it last sent, degrees
  EscortMode mode = EscortMode::follow;
};

// The fighter `f` (in slot `slot`, already set up by `fighter_init`) made a companion of the
// player in slot `leader`: every player its friend, the monsters' models off the server's model
// list, the buttons' rests read. Call after both have joined the map.
void escort_init(Fighter& f, const server::RulesCommon& s, int slot, int leader);
// Every command before the bot's own: the monsters' stirrings, the threats as the senses' foes.
void escort_sense(Fighter& f, const server::RulesCommon& s, int me, double t);
// The think (in place of a fighter's mode and goal): the mode, and the pilot's goal for it.
void escort_think(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, double t,
                  ge::ai::Why* why);
// The command after the bot's own: a button pressed, the boredom's view and keys, the view
// turned at a person's rate.
void escort_command(Fighter& f, const server::RulesCommon& s, int me, double t, int msec,
                    sh::MoveCmd& cmd);
// Whether a living player other than the companion stands within `clear` of the line from `eye`
// to `target`, nearer than it.
bool escort_player_in_line(const Fighter& f, const server::RulesCommon& s, sh::Vec3f eye,
                           sh::Vec3f target, float clear);

}  // namespace shooters_edge::bots
