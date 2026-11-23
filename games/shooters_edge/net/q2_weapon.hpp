// THE WINDOW JOINED TO A SERVER OF QUAKE II, ITS WEAPON PREDICTED: the hand as the newest frame
// left it, run forward over every move the server has not yet answered by Quake II's own weapon
// rules (server/q2_combat.hpp's `sq_hand_step`, the ones our server runs), so the shot's frame,
// its muzzle flash's sounds and the ammunition show when the button goes down, not a round trip
// later. What every game's prediction shares (the shown move, a shot played once, the sound
// mask, the hand fired) is net/weapon_shown.hpp's; the settings are the QuakeWorld join's
// (`cl_nopred_weapon`, `cl_predict_weaponsound`, `cl_predict_buffer`).
//
// IT NEEDS NOTHING FROM THE SERVER past what every server of Quake II's protocols sends to
// predict the weapon in hand: the player state's gun model, gun frame and ammunition stat. A known
// gun model names the weapon; its frame names the hand's state by the weapon's frame runs
// (activate, fire, idle, deactivate); the frame at the end of a fire run is still firing when the
// frame before was a fire frame, else ready. A stock server and ours alike, any protocol (34, 35,
// 36).
//
// THE MOVES AND THE SERVER'S FRAMES: the hand steps at the start of each server frame (every 0.1
// s) unless a move already stepped it in that frame, and a move with attack down steps it at once
// when it arrives. The prediction places each move at the level time its packet reached the server
// (`Q2FramePhase`: packets carry several moves, so moves come in clumps) and the frames at their
// tenths. On a stock server only a new press steps it at once (when the hand has not stepped yet
// that frame); the press is kept to the frame's start, where a ready hand fires on it or on attack
// held, and then dropped (a tap during a shot is lost, as Quake II has it); our server steps it at
// once for any move with attack down, the first of each frame (told by its feel, `*ge_ext` bit
// 64). Each frame steps the hand once either way (porting/shooters-edge/q2-weapon-prediction.md).
//
// SHOTS AND SOUNDS: one step a server frame, so one shot and one frame sound a frame: each is
// named by the frame it is sent with (a frame's own step: that frame; a move's: the next), so one
// a correction moves within its frame is played once. A shot plays its muzzle flash's sounds
// (client/q2_flash_fx.hpp, the hand drawing the flash), silenced when the server's last flash of
// the player's was, or its sound by name where it has no flash (the hand grenade's throw), and
// fires the hand. The hand's frame sounds (the grenade's pin, the chaingun's spin up and down, the
// hyperblaster's wind-down) and the loop it carries (the hums, the spins, a lit grenade's fuse)
// are the rules' own (`sq_hand_sound`, `sq_hand_loop`), played and drawn on the player. The
// server's copies are matched to what was played (net/weapon_shown.hpp's `WeaponPlayed`): matched,
// dropped; a shot the prediction missed plays late rather than not at all.
//
// A WEAPON CHANGE (`use`, `weapnext`, `weapprev`, `weaplast`, `Q2ClientEnd::weapon_asks`) runs
// before the moves of its packet, as the server runs it, and lowers and raises the hand by the
// rules; the new weapon's model and ammunition are drawn. Only to a weapon the window knows held
// with a shot's ammunition (`Q2Known`): our server tells it every weapon and ammunition
// (`*ge_ext` bit 4096, svc 118); a stock server tells only the weapon in hand, so there only a
// `use` or `weaplast` of a weapon seen in hand since the last death, with its ammunition seen, is
// predicted. Knowing every weapon, the rules' own choice when the one in hand runs dry is run too.
//
// THE HAND GRENADE: its fuse is lit at frame 11 and held while attack is; the fuse's time is the
// one the prediction lit (a frame already showing it cooking, the frame's time).
//
// OFF: `cl_nopred_weapon 1`, `cl_nopred 1`, a spectator, dead, a move type but normal, the
// intermission, a gun model the rules do not know (a mod's), more moves unanswered than the ring
// keeps. Off, the drawn state is the server's.
//
// ge:summary The window joined to a server of Quake II: its held weapon predicted from the frame's
// ge:summary gun model, gun frame and ammo over the moves not yet answered
// ge:keywords weapon-prediction q2 q2_weapon_predict gun-frame gun-model press-latch
// ge:keywords predicted-flash muzzle-flash own_shots stock-server weapon-change arsenal
// ge:keywords chaingun-spin hand-grenade fuse hand-loop
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_weapon_test.cpp
#pragma once

#include <cstdint>

#include "shooter/vec.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/weapon_shown.hpp"
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
  bool known = false;    // every weapon and ammunition known: short of a shot, the rules choose
};
// What one step made: a shot (its flash value, -1 none: a sound by name instead, or none when
// `quiet`: a hand grenade let go too late) and the frame sound it played (kSqHandSound's row, -1
// none); the level time and the server frame it is sent with (a frame's own step: that frame; a
// move's: the frame after the one it falls in); the weapon that stepped.
struct Q2HandShot {
  bool shot = false;
  bool quiet = false;
  int flash = -1;
  int sound = -1;
  int weapon = 0;
  double at = 0;
  int32_t frame = 0;
};
// The run over one move the server runs at level time `at`, attack `attack`: the frames that start
// by then stepped first, then the move's own step; its shots written to `out` (at most `room`),
// their count returned.
int q2_hand_move(Q2HandRun& r, double at, bool attack, Q2HandShot* out, int room);

// WHEN THE SERVER RUNS A MOVE: when the packet that first carried it arrives, so a move's level
// time is that packet's send time less the offset between the client's clock and the server's
// frames (`Q2FramePhase`). Each frame says the frame's start fell after the packet it answered
// arrived and before the next one: a window on the offset. A server that reads packets and starts
// frames on its own ticks (ours, every 13 ms) moves each window by up to a tick, so no one window
// holds them all: the phase keeps the last 32 frames' windows and takes the middle of the span
// the most of them share. A changed latency outvotes the old windows within 16 frames.
inline constexpr int kQ2PhaseFrames = 32;
struct Q2FramePhase {
  double lo[kQ2PhaseFrames] = {}, hi[kQ2PhaseFrames] = {};  // s: send time less level time
  int count = 0;       // windows kept (at most kQ2PhaseFrames), the newest at `(count - 1) & 31`
  uint32_t added = 0;  // windows ever added
  int32_t frame = -1;  // the newest frame folded in (-1 none)
};
// Frame `frame` (its start at level time frame × 0.1 s), answering a packet sent at `sent` with
// the next sent at `next` (or none yet: a large number), folded into `p`. A frame with no next
// packet is kept a packet's interval wide (50 ms) only while it is the first.
void q2_phase_frame(Q2FramePhase& p, int32_t frame, double sent, double next);
// The offset the phase holds: the middle of the widest span the most windows share (0 none).
double q2_phase_offset(const Q2FramePhase& p);

// The frames the predicted shots and the server's shots fell in, newest last (a ring of 32 each):
// what a lab holds the prediction's placing to. A shot made at level time t is sent with frame
// ceil(t / 0.1 s): a frame's own step at its start, a move's between the frame before and it.
struct Q2ShotAudit {
  int32_t predicted[32] = {};
  int32_t server[32] = {};
  uint32_t predicted_n = 0, server_n = 0;
  uint32_t filled = 0;  // the server's shots played late: none predicted in or beside their frame
};
// WHAT THE WINDOW KNOWS OF THE PLAYER'S WEAPONS, for a change it predicts: our server tells it
// all (`*ge_ext` bit 4096, `Demo::arsenal_told`); a stock server, only the weapon in hand and its
// ammunition, so a weapon counts as held once seen in hand and an ammunition's count as known
// once seen (until he dies), and a change to anything else is left to the server.
struct Q2Known {
  uint16_t weapons = 0;  // by kSqWeapons' row
  int16_t ammo[7] = {};
  bool ammo_seen[7] = {};
  int held = -1, last = -1;  // the weapon in hand, the one before it
  int32_t frame = -1;        // the newest frame folded in
};

struct Q2Weapon {
  bool stock_rule = false;   // a lab's: a stock server's press rule on ours too
  bool on = false;           // the last pass predicted
  uint32_t played_to = 0;    // the newest move whose shots have played
  float last_shot[16] = {};  // each flash value's last play, the server frame it fell in
  Q2FramePhase phase;        // when the server's frames start on the client's clock
  bool silenced = false;     // the server's last flash of the player's was silenced
  server::SqHand shown;      // the hand at the shown move
  int ammo = 0;              // the ammunition there
  // The sounds played, matched to the server's copies (net/weapon_shown.hpp): kinds -1 a shot's
  // flash, 0.. a hand sound's row (kSqHandSound), 100 a shot heard by name.
  WeaponPlayed played;
  Q2Known known;             // the weapons and ammunition the window knows
  float last_sound[4] = {};  // each hand sound's last play, the server frame it fell in
  double fuse_at = 0;        // the hand grenade's: level time its fuse ends, predicted (0 none)
  Q2ShotAudit audit;         // where the shots fell, predicted and the server's
};

// One pass: the hand run forward from the newest frame over the moves not yet answered, drawn
// into `d` (the player's gun frame, the ammunition), its new shots' sounds added to `d.started`
// and counted in `d.own_shots`, the server's copies dropped. `w` holds the settings; `nopred`
// is `cl_nopred`; `origin` the player's drawn place.
void q2_weapon_predict(Q2Weapon& p, const RemoteWeapon& w, bool nopred, const Q2ClientEnd& c,
                       client::Demo& d, const ge::shooter::Vec3f& origin);

}  // namespace shooters_edge::net
