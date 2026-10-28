// THE CLIENT'S PREDICTION: what the client shows of its own player before the server has answered,
// and of the other players between the server's frames.
//
// ITS OWN PLAYER (`predict_player`): from the last frame the server acknowledged (the reply to
// command S), the player's state AS THE PROTOCOL CARRIES IT (`NetPlayer`: the origin on a 1/8
// grid, each coordinate x 8 truncated toward zero; the velocity in whole units, truncated toward
// zero; the move type, jump held and on the ground), every command after S up to the newest is
// run through the same player move as the server (`run_command`, the 50 ms split included) over
// the client's solids (the world, the brush models, the other players' boxes at their predicted
// places: `client_world`). The move's unstick step is what lets a grid position a hair inside a
// wall start cleanly. With nothing after S (or prediction off) the server's state is used as is.
//
// SMOOTHING (`Smoothing`): a prediction error of 4 to 64 units seen when a frame arrives (the
// newest command's prediction before the frame against after it) is not snapped but folded into a
// correction drawn on top of the prediction, which decays at 200 units/s below 8 units, 500 below
// 16, 800 below 32 and 1400 above, times a smoothing setting. A smaller error is taken at once;
// a larger one (a teleport) snaps and clears the correction.
//
// OTHER PLAYERS (`predict_other`): run forward from their last state by how long ago the server
// last ran their command: the server sends each player's milliseconds since its last command
// (`net_msec`, at most 255); the state's time is the frame's arrival minus that; the player time is
// now minus the measured latency plus 0.02 s (never in the future); that player's last command is
// run for the difference (`other_msec`, at most 255, split as usual).
//
// The quantizing is the protocol's and the move is the server's, so a prediction differs from the
// server only by what the grid dropped from the acknowledged state (under 1/8 unit and 1 unit/s,
// grown by the commands replayed over it) and by what the client could not know (a command the
// speed check cut, a door's or another player's place): the se-loop lab charts it.
//
// ge:summary The client's prediction: the protocol's player state, replayed commands, smoothing
// ge:keywords prediction predict client reconcile replay acknowledged quantize grid net-player
// ge:keywords smoothing correction decay other-players extrapolate latency predict_player
// ge:keywords predict_other client_world net_player_of net_msec other_msec
// ge:skill shooter net
// ge:test tests/shooter_predict_test.cpp
#pragma once

#include <cstdint>

#include "shooter/server.hpp"

namespace ge::shooter {

constexpr float kSmoothMin = 4;   // units: a smaller error is taken at once
constexpr float kSmoothMax = 64;  // units: a larger one snaps
constexpr double kOtherLead = 0.02;  // s: other players drawn this much past now - latency

// A player's state as the protocol carries it.
struct NetPlayer {
  int32_t origin8[3] = {0, 0, 0};    // origin x 8, truncated toward zero
  int16_t velocity[3] = {0, 0, 0};  // units/s, truncated toward zero
  MoveType type = MoveType::normal;
  bool jump_held = false;
  bool onground = false;
};

// The protocol's copy of a move state, and the move state a client starts from.
NetPlayer quantize_player(const MoveState& s);
MoveState unquantize_player(const NetPlayer& n, const MoveSettings& set);

// What the server would send of player slot `slot` after its frame, and the ms since it last ran
// that player's command (at most 255).
NetPlayer net_player_of(const Server& sv, int slot);
int net_msec(const Server& sv, int slot);

// A brush model where the client last saw it.
struct ClientModel {
  const BrushModel* model = nullptr;
  Vec3f origin;
};

// The client's solids: the world, the brush models, the other players' boxes (kPlayerMins..
// kPlayerMaxs at each origin), in that order, at most kMaxSolids.
void client_world(const BrushModel& world, const ClientModel* models, int model_count,
                  const Vec3f* players, int player_count, MoveWorld& out);

// Its own player: the acknowledged state with `count` later commands run through the move.
MoveState predict_player(const NetPlayer& acked, const MoveCmd* cmds, int count,
                         const MoveSettings& set, const MoveWorld& world);

// The milliseconds an other player is run forward (0..255).
int other_msec(double arrival_s, int state_msec, double now_s, double latency_s);
// An other player run forward by `msec` with its last command.
MoveState predict_other(const NetPlayer& state, MoveCmd last_cmd, int msec,
                        const MoveSettings& set, const MoveWorld& world);

struct Smoothing {
  Vec3f correction;  // drawn on top of the prediction, units
  float scale = 1;   // the smoothing setting: how fast the correction decays
};
// A frame arrived: the newest command's prediction before it and after it.
void smoothing_frame(Smoothing& s, Vec3f before, Vec3f after);
// The correction decays over `dt` seconds of render time.
void smoothing_decay(Smoothing& s, double dt);
inline Vec3f smoothed(const Smoothing& s, Vec3f predicted) { return predicted + s.correction; }

}  // namespace ge::shooter
