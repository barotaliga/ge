// QUAKE II'S POSES: the frame each body shows, stepped once a rules frame (every 100 ms) after
// the frame's moves, shots and blows. Nothing here changes the game: a pose only names a frame of
// the body's model for the world's row (and so the wire and the window).
//
// - **Players** (the player's model, 198 frames): a sequence plays one frame a rules frame from
//   its first to its last, then the body goes back to its basic loop: standing, running (any
//   sideways speed), crouched standing or crouched walking. A sequence has a priority, lowest
//   first: basic, gesture (the landing), jump, pain, attack, death, reverse (played backwards);
//   a new one starts only over a lower one, except a shot, which cuts everything below death. In
//   the air the body shows its first jump frame, then holds the second; landing plays the last
//   four. A shot plays the attack (the crouched attack when ducked); a hand grenade's throw
//   plays the wave backwards standing. A blow taken plays a pain, the three standing pains in
//   turn; a death one of three deaths in turn (or the crouched death), held on its last frame,
//   which a corpse keeps. Putting a weapon away plays the third pain backwards over its last four
//   frames; bringing one up (and every spawn) plays it forwards from its second frame. Crouching
//   or standing up cuts everything below death.
// - **Monsters**: a kind's model has its runs of frames for standing, walking, running, each of
//   its attacks, its pains and its deaths (`SqMonsterPoses`, measured from the models' frame
//   names). Standing, walking (no enemy) and running (an enemy) loop by how it moved this frame; an
//   attack starts when the monster begins one and loops while the attack lasts, holding its firing
//   frame through a burst where it has one; a pain plays once over the basic loop, never over an
//   attack; a death plays once and holds its last frame, which the corpse keeps.
//
// ge:summary Quake II's poses: players' and monsters' frames by what they do, stepped at 10 Hz
// ge:keywords q2 poses animation frames player-animation monster-animation priority pain death
// ge:keywords jump landing attack crouch corpse frame-table sq_monster_poses pose_player
// ge:skill shooter
// ge:test tests/shooters_edge_q2_poses_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "shooter/vec.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

struct Q2Server;
struct Q2Monster;

// A run of a model's frames, first to last (both shown); `first` -1: none.
struct SqSpan {
  int16_t first = -1, last = -1;
  constexpr bool none() const { return first < 0; }
};

// The player's model's frames.
namespace sq_frames {
inline constexpr SqSpan kStand{0, 39}, kRun{40, 45}, kAttack{46, 53};
inline constexpr SqSpan kPain[3] = {{54, 57}, {58, 61}, {62, 65}};
inline constexpr SqSpan kJump{66, 71};
inline constexpr SqSpan kFlip{72, 83}, kSalute{84, 94}, kTaunt{95, 111};
inline constexpr SqSpan kWave{112, 122}, kPoint{123, 134};
// The five gestures `wave <n>` plays (q2_commands.hpp), by n: flip off, salute, taunt, wave,
// point. The spans are the model's frame names counted in its own order (measured).
inline constexpr SqSpan kGestures[5] = {kFlip, kSalute, kTaunt, kWave, kPoint};
inline constexpr SqSpan kCrouchStand{135, 153}, kCrouchWalk{154, 159}, kCrouchAttack{160, 168};
inline constexpr SqSpan kCrouchPain{169, 172}, kCrouchDeath{173, 177};
inline constexpr SqSpan kDeath[3] = {{178, 183}, {184, 189}, {190, 197}};
}  // namespace sq_frames

enum class SqPriority : uint8_t { basic, gesture, jump, pain, attack, death, reverse };

// A player's pose: the frame shown, the sequence's end, and what the frame asked of it.
struct SqPose {
  int frame = 0;
  int end = 0;  // the sequence's last frame (its first, played backwards)
  SqPriority priority = SqPriority::basic;
  bool ducked = false, running = false;
  // Asked during the frame (by a shot, a throw), read and cleared by the frame's step.
  bool shot = false, threw = false;
  // A gesture asked by `wave` (0..4, sq_frames::kGestures; -1 none), started by the next step.
  int gesture = -1;
  // What the last step saw: his health and armour (a blow taken), the weapon in hand.
  float health = 0;
  int armour = 0;
  int weapon = 0;
};
// Whether a gesture may start now: standing (not crouched) and nothing above a gesture playing.
bool sq_can_gesture(const SqPose& p, bool on_ground);

// A kind's runs of frames. An attack is by the monster's attack index (0 and 1 its blows, 2 to 4
// its ranged attacks), two runs where it picks one at even odds; `hold` the frame it holds while
// a burst lasts (-1 none).
struct SqMonsterPoses {
  const char* classname;
  SqSpan stand, walk, run;
  SqSpan attack[5][2] = {};
  int16_t hold[5] = {-1, -1, -1, -1, -1};
  SqSpan pain[3] = {};
  SqSpan death[6] = {};
  // Its fidget standing: started by its idle call (`fidget_odds` 0), or reaching one of
  // `fidget_at`'s stand frames with `fidget_odds` in a hundred (never while it holds its ground).
  SqSpan fidget = {};
  int8_t fidget_odds = 0;
  int16_t fidget_at[3] = {-1, -1, -1};
  SqSpan stand2 = {};  // a second stand, picked at even odds each time it stands
};

// The kind's poses by its class name; null for one with none (it keeps its frame).
const SqMonsterPoses* sq_monster_poses(std::string_view classname);

// What a monster shows: the run it plays, the frame, and what the frame asked of it.
struct SqMonsterPose {
  enum class Act : uint8_t { stand, walk, run, attack, pain, death, duck } act = Act::stand;
  SqSpan span;
  int frame = 0;
  int slot = 0;             // the attack it plays (its index)
  int cue_attack = -1;      // an attack begun this frame (its index); -1 none
  bool cue_pain = false;    // a flinch this frame
  int shown = -1;           // an attack frame its think plays by hand this frame; -1 none
  SqSpan shown_run;         // the run that frame belongs to
  SqSpan pain_run;          // the flinch's run its kind's pains chose (none: one at random)
  bool cue_fidget = false;  // its idle call this frame: its fidget, standing
  sh::Vec3f last;           // where it stood at the last step
  int heard = -1;           // the frame whose footstep was last weighed (g_monsterfootsteps)
};

// A player's pose stepped once a rules frame, at its end.
void pose_player(Q2Server& s, int slot);
// A spawned player's pose: standing, his first weapon coming up.
void pose_spawn(Q2Server& s, int slot);
// A monster's pose stepped once a rules frame, after its think.
void pose_monster(Q2Server& s, Q2Monster& m);

}  // namespace shooters_edge::server
