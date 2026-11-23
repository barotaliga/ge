// THE HELD WEAPON PREDICTED, WHAT EVERY GAME'S JOIN SHARES: a game's weapon rules run forward on
// the client over the commands the server has not yet answered (QuakeWorld's in
// net/qw_weapon.hpp, Quake II's in net/q2_weapon.hpp), so a shot is drawn and heard when the
// button goes down. The rules are the game's; what is decided here is the same in every game:
//
// - THE SHOWN COMMAND (`weapon_shown_command`): the one `cl_predict_buffer` + 1 before the next to
//   make (1: the second newest), never past the newest, never before the second after the last
//   one the server answered. The weapon drawn, its frame and the ammunition are the prediction's
//   at that command; each pass rebuilds them from the server's state, so the server corrects a
//   mistake at its next frame.
// - A SHOT PLAYED ONCE (`weapon_shot_fresh`): a shot is played when the shown command first
//   reaches its command (`played_to`), and one a correction moves is not played twice: one of the
//   same kind within `gap` of game time of the last is the same shot.
// - WHICH SOUNDS THE PREDICTION PLAYS (`weapon_sound_plays`, `cl_predict_weaponsound`): 0 none (the
//   server's are heard), 1 every one; above 1 its bits name the weapons left to the server, bit
//   2^n the weapon on key n (QuakeWorld: 2 the axe .. 256 the lightning gun; Quake II: 2 the
//   blaster .. 512 the railgun, 1024 the BFG10K).
// - THE HAND FIRED (`weapon_shot_fired`): each predicted shot counts in `DemoState::own_shots`,
//   which the window's hand fires on (client/held.cpp).
// - THE FRAME DRAWN only once the server has sent a weapon frame other than 0 (`frames_live`):
//   rules that never animate it leave the hand to play its own shot.
//
// ge:summary The held weapon predicted, the part every game shares: the shown command, a shot
// ge:summary played once, the sound mask by weapon key, the hand fired
// ge:keywords weapon-prediction shown-command cl_predict_buffer cl_predict_weaponsound
// ge:keywords cl_nopred_weapon same-shot played_to own_shots weapon-key sound-mask
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_weapon_test.cpp
#pragma once

#include <cstdint>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::net {

// The shown command: `next` is the next command to make (not yet made), `answered` the newest the
// server has answered; `buffer` the commands held back (`cl_predict_buffer`).
inline uint32_t weapon_shown_command(uint32_t answered, uint32_t next, int buffer) {
  uint32_t shown = next - 1 - static_cast<uint32_t>(buffer > 0 ? buffer : 0);
  if (static_cast<int32_t>(shown - (answered + 2)) < 0) shown = next - 1;
  return shown;
}

// Whether the prediction plays a sound of the weapon on key `key` under `cl_predict_weaponsound`
// `mask` (above).
inline bool weapon_sound_plays(int mask, int key) {
  return mask == 1 || (mask > 1 && (mask & (1 << key)) == 0);
}

// Whether a shot of `kind` at game time `clock` is new: none of that kind within `gap` of the last
// (`last[kind]`, 0 none yet), which it then becomes. `last` holds `kinds` entries.
inline bool weapon_shot_fresh(float* last, int kinds, int kind, float clock, float gap) {
  if (kind < 0 || kind >= kinds) return true;
  float& l = last[kind];
  if (l != 0 && clock < l + gap && clock > l - gap) return false;
  l = clock;
  return true;
}

// A predicted shot: the window's hand fires on it, from weapon frame `frame`.
inline void weapon_shot_fired(client::DemoState& d, int frame) {
  d.own_shots++;
  d.own_shot_frame = frame;
}

}  // namespace shooters_edge::net
