// THE WINDOW JOINED TO A SERVER OF QUAKE II, ITS WEAPON PREDICTED: the hand as the newest frame
// left it, run forward over every move the server has not yet answered by Quake II's own weapon
// rules (server/q2_combat.hpp's `sq_hand_step`, the ones our server runs), so the shot's frame,
// its muzzle flash's sounds and the ammunition show when the button goes down, not a round trip
// later. What every game's prediction shares (the shown move, a shot played once, the sound
// mask, the hand fired) is net/weapon_shown.hpp's; the settings are the QuakeWorld join's
// (`cl_nopred_weapon`, `cl_predict_weaponsound`, `cl_predict_buffer`).
//
// IT NEEDS NOTHING FROM THE SERVER past what every server of Quake II's protocols sends: the
// player state's gun model, gun frame and ammunition stat. A known gun model names the weapon;
// its frame names the hand's state by the weapon's frame runs (activate, fire, idle, deactivate);
// the frame at the end of a fire run is still firing when the frame before was a fire frame, else
// ready. A stock server and ours alike, any protocol (34, 35, 36).
//
// THE MOVES AND THE SERVER'S FRAMES: the hand steps at the start of each server frame (every 0.1
// s) unless a move already stepped it in that frame, and a move with attack down steps it at once
// when it arrives. The prediction places each move at the level time its packet reached the server
// (`Q2FramePhase`: packets carry several moves, so moves come in clumps) and the frames at their
// tenths. On a stock
// server only a new press steps it at once (when the hand has not stepped yet that frame); the
// press is kept to the frame's start, where a ready hand fires on it or on attack held, and then
// dropped (a tap during a shot is lost); our server steps it at once for any move with attack
// down, the first of each frame (told by its feel, `*ge_ext` bit 64). Each frame steps the hand
// once either way (porting/shooters-edge/q2-weapon-prediction.md).
//
// SHOTS: one a server frame at most (the hand steps once a frame), so a shot a correction moves
// to another move of the same frame is the same shot and played once. A shot plays its muzzle
// flash's sounds (client/q2_flash_fx.hpp, the hand drawing the flash), silenced when the server's
// last flash of the player's was, and fires the hand. The server's own copies (its muzzle flashes
// of the player's weapon shots, `DemoSound::flash`) are dropped while predicting. The chain gun's
// spin, the hand grenade's pin and the hyperblaster's wind-down stay the server's.
//
// OFF: `cl_nopred_weapon 1`, `cl_nopred 1`, a spectator, dead, a move type but normal, the
// intermission, a gun model the rules do not know (a mod's), the hand grenade in hand, the
// ammunition short of a shot (the server's choice of the next weapon is not guessed), more moves
// unanswered than the ring keeps. Off, the drawn state is the server's.
//
// Not predicted: a weapon change (the client does not know what the player holds).
//
// ge:summary The window joined to a server of Quake II: its held weapon predicted from the frame's
// ge:summary gun model, gun frame and ammo over the moves not yet answered
// ge:keywords weapon-prediction q2 q2_weapon_predict gun-frame gun-model press-latch
// ge:keywords predicted-flash muzzle-flash own_shots stock-server
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_weapon_test.cpp
#pragma once

#include <cstdint>

#include "shooter/vec.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/server/q2_combat.hpp"

namespace shooters_edge::net {

struct Q2ClientEnd;
struct RemoteWeapon;

// The hand a frame shows: weapon `weapon` (kSqWeapons' row) at gun frame `frame`, `before` the
// gun frame of the frame before it (-1 unknown).
server::SqHand q2_hand_of_frame(int weapon, int frame, int before);

// The weapon (kSqWeapons' row) whose view model is `model`; -1 none.
int q2_weapon_of_model(const char* model);

// What a run of the rules over moves does, a move at a time (the server's frame and move steps).
struct Q2HandRun {
  server::SqHand hand;
  server::SqBody body;
  bool latch = false;    // a stock server: only a new press steps at once
  bool pressed = false;  // a stock server's press kept to the frame's start
  bool held = false;     // attack down in the last move run
  bool stepped = false;  // the hand stepped this frame
  double frame_at = 0;   // s: the level time of the frame the moves are in
};
// One step's shot: its flash value (-1 none) and the level time.
struct Q2HandShot {
  int flash = -1;
  double at = 0;
};
// The run over one move the server runs at level time `at`, attack `attack`: the frames that start
// by then stepped first, then the move's own step; its shots written to `out` (at most `room`),
// their count returned.
int q2_hand_move(Q2HandRun& r, double at, bool attack, Q2HandShot* out, int room);

// WHEN THE SERVER RUNS A MOVE: when the packet that first carried it arrives, so a move's level
// time is that packet's send time less the offset between the client's clock and the server's
// frames (`Q2FramePhase`). Each frame says the frame's start fell after the packet it answered
// arrived and before the next one: a window on the offset, narrowed by every frame since the last
// that disagreed. The prediction takes its middle.
struct Q2FramePhase {
  double lo = 0, hi = 0;  // s: the offset's window (send time less level time)
  int32_t frame = -1;     // the newest frame folded in (-1 none)
  bool have = false;
};
// Frame `frame` (its start at level time frame × 0.1 s), answering a packet sent at `sent` with
// the next sent at `next` (or none yet: a large number), folded into `p`.
void q2_phase_frame(Q2FramePhase& p, int32_t frame, double sent, double next);
// The offset the phase holds (its window's middle).
inline double q2_phase_offset(const Q2FramePhase& p) { return (p.lo + p.hi) * 0.5; }

struct Q2Weapon {
  bool stock_rule = false;   // a lab's: a stock server's press rule on ours too
  bool on = false;           // the last pass predicted
  uint32_t played_to = 0;    // the newest move whose shots have played
  float last_shot[16] = {};  // each flash value's last play, the server frame it fell in
  Q2FramePhase phase;        // when the server's frames start on the client's clock
  bool silenced = false;     // the server's last flash of the player's was silenced
  server::SqHand shown;      // the hand at the shown move
  int ammo = 0;              // the ammunition there
};

// One pass: the hand run forward from the newest frame over the moves not yet answered, drawn
// into `d` (the player's gun frame, the ammunition), its new shots' sounds added to `d.started`
// and counted in `d.own_shots`, the server's copies dropped. `w` holds the settings; `nopred`
// is `cl_nopred`; `origin` the player's drawn place.
void q2_weapon_predict(Q2Weapon& p, const RemoteWeapon& w, bool nopred, const Q2ClientEnd& c,
                       client::Demo& d, const ge::shooter::Vec3f& origin);

}  // namespace shooters_edge::net
