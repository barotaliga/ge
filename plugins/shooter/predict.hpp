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
// a larger one (a teleport) snaps and clears the correction. A game's client may cut the error by
// its latency, drop a correction too small to see, or turn it off (`Smoothing`'s fields).
//
// OTHER PLAYERS (`predict_other`): run forward from their last state by how long ago the server
// last ran their command: the server sends each player's milliseconds since its last command
// (`net_msec`, at most 255); the state's time is the frame's arrival minus that; the player time is
// now minus the measured latency plus 0.02 s (never in the future); that player's last command is
// run for the difference (`other_msec`, at most 255, split as usual). The timing rule is any
// game's: a game with its own move (Quake II's, net/q2_predict.hpp) takes `other_msec` and
// runs its own move for it.
//
// The quantizing is the protocol's and the move is the server's, so a prediction differs from the
// server only by what the grid dropped from the acknowledged state (under 1/8 unit and 1 unit/s,
// grown by the commands replayed over it) and by what the client could not know (a command the
// speed check cut, another player's place): the se-loop lab charts it. A door's, a plat's or a
// train's place is run on with the replay (`advance_movers`, below); the se-control lab charts what
// is left.
//
// ge:summary The client's prediction: the protocol's player state, replayed commands, smoothing
// ge:keywords prediction predict client reconcile replay acknowledged quantize grid net-player
// ge:keywords smoothing correction decay other-players extrapolate latency predict_player
// ge:keywords advance_movers mover-ahead rider plat door
// ge:keywords predict_other client_world net_player_of net_msec other_msec
// ge:skill shooter net
// ge:test tests/shooter_predict_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/server.hpp"

namespace ge::shooter {

constexpr float kSmoothMin = 4;         // units: a smaller error is taken at once
constexpr float kSmoothMax = 64;        // units: a larger one snaps
constexpr double kSmoothStepS = 0.013;  // s: the error a step's timing explains (the latency cut)
constexpr double kOtherLead = 0.02;     // s: other players drawn this much past now - latency

// A player's state as the protocol carries it.
struct NetPlayer {
  int32_t origin8[3] = {0, 0, 0};   // origin x 8, truncated toward zero
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

// A brush model where the client last saw it, and how fast it was going there (units/s: its move
// between the last two frames over their gap; 0 for one standing still).
struct ClientModel {
  const BrushModel* model = nullptr;
  Vec3f origin;
  Vec3f velocity;
  float reach = 1e30f;  // units it may run on before it stops (`mover_reach`)
};

// How far a mover at `origin` going at `velocity` runs on before it stops: to the nearest of
// `rests` (places it was seen standing: a door shut or open, a plat at the top or the bottom)
// that lies ahead of it on its line (within a unit of it), else unbounded (1e30).
float mover_reach(Vec3f origin, Vec3f velocity, const Vec3f* rests, int count);

// A MOVER'S MOTION as a client reads it off the frames (`MoverTrack`): each entity's place in the
// last two frames and their times give its velocity (none across a change of model or a jump of
// more than `kMoverJump` units: a train's teleport, a respawn); the places it was seen standing
// (the same place two frames running, the newest four) and the place it was built (the origin: a
// door shut, a plat at the top) are where it stops (`mover_reach`).
inline constexpr float kMoverJump = 200;
struct MoverTrack {
  struct One {
    Vec3f prev, last;
    int prev_model = -1, last_model = -1;  // -1: not in that frame
    Vec3f rests[4];
    int rest_count = 0, rest_next = 0;
  };
  std::vector<One> ents;
  double prev_s = -1, last_s = -1;  // the two frames' times
};
// A new frame at `time_s` (the server's time, or the network's): the last becomes the previous.
void mover_track_frame(MoverTrack& t, double time_s);
// Entity `entity` in the new frame: brush model `model` at `origin`.
void mover_track_seen(MoverTrack& t, size_t entity, int model, Vec3f origin);
// Entity `entity`'s velocity at its newest place, and how far it runs on (none: 0 and 1e30).
void mover_motion(const MoverTrack& t, size_t entity, Vec3f& velocity, float& reach);

// Where a mover at `origin` going at `velocity` is `seconds` on, stopped after `reach`: the place
// the window draws it so the movers it shows agree with the player's own predicted place.
Vec3f mover_ahead(Vec3f origin, Vec3f velocity, float reach, float seconds);

// The client's solids: the world, the brush models, the other players' boxes (kPlayerMins..
// kPlayerMaxs at each origin), in that order, at most kMaxSolids.
void client_world(const BrushModel& world, const ClientModel* models, int model_count,
                  const Vec3f* players, int player_count, MoveWorld& out);

// MOVERS RUN AHEAD (`advance_movers`): a replayed command ran on the server later than the frame
// the client last saw, when a door, a plat or a train had gone on moving. Before each command the
// brush models `models` (solids 1.. of `world`, as `client_world` lays them) move on by their
// velocity times the command's time, and the player goes with one as the server's push takes him:
// carried when he stands on it (`s.ground` its solid), shoved when it moved into him; either only
// where his box is free after. A mover stops after its `reach`.
void advance_movers(MoveWorld& world, MoveState& s, const ClientModel* models, int count, float dt);

// Its own player: the acknowledged state with `count` later commands run through the move.
// `each` (when given, `count` of them): the state after each command, in order (a jump's command
// is the one whose `jump_held` turned on, `move_jumped`).
MoveState predict_player(const NetPlayer& acked, const MoveCmd* cmds, int count,
                         const MoveSettings& set, const MoveWorld& world,
                         MoveState* each = nullptr);
// The command that took the state from `before` to `after` jumped: the jump button, unheld since
// the last jump, was taken on the ground (`jump_held` turned on from standing; a swim stroke or a
// water jump leaves it off, a death holds it without a jump).
inline bool move_jumped(const MoveState& before, const MoveState& after) {
  return !before.jump_held && after.jump_held && before.ground != -1 &&
         after.type != MoveType::dead;
}

// The milliseconds an other player is run forward (0..255).
int other_msec(double arrival_s, int state_msec, double now_s, double latency_s);
// An other player run forward by `msec` with its last command.
MoveState predict_other(const NetPlayer& state, MoveCmd last_cmd, int msec, const MoveSettings& set,
                        const MoveWorld& world);

struct Smoothing {
  Vec3f correction;  // drawn on top of the prediction, units
  float scale = 1;   // the smoothing setting: how fast the correction decays
  bool off = false;  // every error taken at once (no correction kept)
  // units/s: a correction left smaller than this times `scale` and the step is dropped (0: never).
  float drop = 0;
};
// A frame arrived: the newest command's prediction before it and after it. With `latency_s` over
// 0 the error folded in is cut by the share of it a frame of 0.013 s explains (1 - 0.013 /
// latency, none at or under 0.013 s): the part a mere step's timing makes is taken at once.
void smoothing_frame(Smoothing& s, Vec3f before, Vec3f after, double latency_s = 0);
// The correction decays over `dt` seconds of render time.
void smoothing_decay(Smoothing& s, double dt);
inline Vec3f smoothed(const Smoothing& s, Vec3f predicted) { return predicted + s.correction; }

}  // namespace ge::shooter
