// TELEPORTS FORESEEN BY THE CLIENT: a teleporter whose box and destination the client knows from
// the level's entities is taken in the client's own prediction the command the player's box
// reaches it, so the window draws him at the far end and turns his view the frame he walks in,
// not a round trip later when the server's word arrives. The server's physics are untouched: the
// client runs what the server will run.
//
// THE TABLE (`Teleporter`): a box the server tests the player's box against (its own growth
// included: `teleporter_at` adds the player's), where the player is put, the view he is turned
// to, his velocity on arrival and a hold the move runs (Quake II's teleport pause; 0 none). A game
// fills it from its own entities (plugins/classic/teleporters.hpp: Quake's and Quake II's).
//
// PUSHES (`push`): a push trigger (a wind tunnel, a jump pad) is an entry that only sets the
// player's velocity, every command his box is in it, as the server's touch does after his move:
// without it the window draws him walking on for a round trip and is then yanked along the push.
// `pusher_at` finds them; `teleporter_at` passes over them. A push makes no guess: the server's
// state carries the velocity it gave, and a wrong one is an ordinary correction.
//
// THE GUESS (`TeleGuess`): one predicted teleport the server has not yet answered: the command
// that touched it, which teleporter, and the turn the view was given. The server's answer settles
// it:
//
// - its acknowledged state reaches the guess's command with the player nearer the destination than
//   the teleporter: kept (it was right; a turn from the server that matches is the same one);
// - two commands past it with the player still by the teleporter: wrong (a teleporter waiting to be
//   switched on, a mod's own rule). The view's turn is undone, keeping the player's own turning,
//   and that teleporter is left out of the prediction while the server has him in its box;
// - a turn from the server to another view (another teleporter, a respawn): the server's view.
//
// ge:summary Teleports a client foresees: the table, the prediction's hit, the guess and its answer
// ge:keywords teleport teleporter predict-teleport teleport-prediction cl_predict_teleport guess
// ge:keywords tele_guess teleporter_at teleport_state predict_player hold confirm undo
// ge:keywords trigger_push push-trigger jump-pad wind-tunnel pusher_at predict-push cl_predict_push
// ge:skill shooter net
// ge:test tests/shooter_teleport_ahead_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/pmove.hpp"
#include "shooter/predict.hpp"
#include "shooter/vec.hpp"

namespace ge::shooter {

struct Teleporter {
  Vec3f lo, hi;       // the box the server tests, its growth included
  Vec3f dest;         // the player's origin on arrival
  Vec3f angles;       // the view he is turned to (pitch, yaw, roll)
  Vec3f velocity;     // his velocity on arrival
  int hold_ms = 0;    // a hold the move runs on arrival (Quake II's); 0 none
  bool push = false;  // a push trigger: only `velocity` is set, the player left where he is
};
using Teleporters = std::vector<Teleporter>;

// The teleporter whose box the player's (`origin` + `mins`..`maxs`, grown by `grow` a side)
// overlaps, the first in the table; -1 none. `skip`: one left out. Pushes are passed over.
int teleporter_at(const Teleporters& t, Vec3f origin, Vec3f mins, Vec3f maxs, float grow,
                  int skip = -1);
// The push trigger the player's box overlaps, the first in the table; -1 none.
int pusher_at(const Teleporters& t, Vec3f origin, Vec3f mins, Vec3f maxs, float grow);

// A level's tables, so the teleports and the pushes switch on and off apart: the teleporters
// alone (the guess's indices), the same teleporters followed by the pushes (the same indices),
// and the pushes alone. `table` picks the one the switches ask for; null when it is empty.
struct TeleTables {
  Teleporters tele, with_push, push_only;
  void fill(Teleporters teleporters, const Teleporters& pushes);
  const Teleporters* table(bool teleports, bool pushes) const;
};

// Quake's arrival on the shared move's state: put there with the velocity, off the ground.
void teleport_state(MoveState& s, const Teleporter& t);

// A replay's teleport: the first command (its place in the replay) whose move ended in a
// teleporter, and which one; -1 none.
struct TeleHit {
  int cmd = -1;
  int index = -1;
};

inline constexpr float kTeleLinkGrow = 1;  // the server grows every box it links by a unit a side

// predict_player (shooter/predict.hpp) taking the teleporters in `tele` (none when null): the
// player's box tested after each command, a push's velocity then a teleport taken as the server
// takes them; `movers` (when given, `mover_count` of them, solids 1.. of `world`) run on before
// each command (`advance_movers`): to `mover_at[i]` seconds after the frame they were seen in
// before command i (when given: when the server will run it), else by each command's time.
MoveState predict_player_teleports(const NetPlayer& acked, const MoveCmd* cmds, int count,
                                   const MoveSettings& set, const MoveWorld& world, MoveState* each,
                                   const Teleporters* tele, int skip, TeleHit* hit,
                                   const ClientModel* movers = nullptr, int mover_count = 0,
                                   const float* mover_at = nullptr);

struct TeleGuess {
  uint32_t seq = 0;     // the command that teleported, 0 none standing
  int index = -1;       // which teleporter (kept after an undo, for a late turn from the server)
  Vec3f turn;           // what the view was turned by (pitch, yaw)
  bool undone = false;  // the server said no and the turn was taken back
  int skip = -1;        // a teleporter the server disagreed with, while he stands in it
};

inline constexpr uint32_t kTeleUndoAfter = 2;  // commands past the guess before a no is believed
inline constexpr float kTeleSameDeg = 2;       // a server turn this near the guess's is the same

// A replay's hit at command `seq` (made with `cmd_angles`): a new one turns `view` to the
// teleporter's angles keeping the turning made since that command (the pitch the teleporter's);
// the change is returned and kept. The same guess again changes nothing.
Vec3f tele_guess_hit(TeleGuess& g, const Teleporters& t, uint32_t seq, int index, Vec3f cmd_angles,
                     Vec3f& view);

// The server answered command `acked` with the player at `server_origin` (his box `mins`..`maxs`).
// Returns the change made to `view` (an undone turn), zero otherwise.
Vec3f tele_guess_acked(TeleGuess& g, const Teleporters& t, uint32_t acked, Vec3f server_origin,
                       Vec3f mins, Vec3f maxs, Vec3f& view);

// The server's own turn of the view to `server_angles` (a setangle). True when it is the guess's
// (the view kept as it is, or turned again after a wrong undo); false: not the guess's, nothing
// changed and the caller takes the server's view as it would without a guess.
bool tele_guess_setangle(TeleGuess& g, const Teleporters& t, Vec3f server_angles, Vec3f& view);

}  // namespace ge::shooter
