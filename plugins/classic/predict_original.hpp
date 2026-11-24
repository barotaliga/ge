// THE CLIENT'S PREDICTION ON THE ORIGINAL MOVE: what a client shows of its own player while the
// server runs the original move (`sv_playermove original` on the multiplayer clock, where each
// command is one original step of its own length: framemove.hpp). The arena move's prediction
// (predict.hpp) runs the predicted move over the client's solids; this one runs the server's own
// original step over a small stand-in server, so the prediction is the server's arithmetic.
//
// THE STAND-IN (`OriginalPrediction`): a server of the level's brush models with no program, one
// client slot (the player, entity 1), the movers as solid brush entities where the client last saw
// them and the other players as boxes. It is built once a level (`original_prediction_level`:
// the models are copied into it) and placed before each prediction (`original_prediction_place`).
//
// ONE PREDICTION (`predict_player_original`): the player stood at the acknowledged state as the
// protocol carries it (`NetPlayer`: the origin on its 1/8 grid, the velocity in whole units, on the
// ground, the jump held), its last free spot that origin; then every later command, split into
// pieces of at most 50 ms as the server splits it, runs the three parts of the server's piece: the
// think (angles and wish), the engine's jump for a multiplayer program (a single-player program
// jumps in its own code, which the client does not run), and the move. What the client cannot know
// is left out: the program's pre-think and think, a water jump's direction and time, the view's
// punch. The se-loop lab's `--move=original` charts the corrections a match's clients make.
//
// ge:summary The client's prediction on the original move: the server's own step on a stand-in
// ge:keywords prediction predict client original-move sv_playermove stand-in-server framemove
// ge:keywords predict_player_original original_prediction_level original_prediction_place
// ge:keywords reconcile acknowledged net-player
// ge:skill classic shooter net
// ge:test tests/classic_predict_original_test.cpp
#pragma once

#include <vector>

#include "shooter/predict.hpp"
#include "shooter/server.hpp"
#include "shooter/teleport_ahead.hpp"

namespace ge::classic {

using ge::shooter::BrushModel;
using ge::shooter::MoveCmd;
using ge::shooter::MoveState;
using ge::shooter::NetPlayer;
using ge::shooter::Server;
using ge::shooter::ServerSettings;
using ge::shooter::Vec3f;

// A brush model by its index among the level's models (`*k`: 0 the world), where it stands, and
// its velocity there (units/s; 0 standing still).
struct ModelAt {
  int model = 0;
  Vec3f origin;
  Vec3f velocity;
  float reach = 1e30f;  // units it runs on before it stops (shooter/predict.hpp, `mover_reach`)
};

struct OriginalPrediction {
  Server sv;
  int placed = 0;            // entities after the player in use: movers, then players
  int movers = 0;            // of them, the movers (entities 2..)
  Vec3f mover_velocity[64];  // each mover's, run on before each command by the server's push
  float mover_reach[64] = {};
};

// The stand-in for a level: `models[k]` is `*k` (0 the world); `set` the server's settings as the
// client knows them (the move's, the original move's own, maxvelocity, who jumps).
void original_prediction_level(OriginalPrediction& p, const std::vector<BrushModel>& models,
                               const ServerSettings& set);

// The movers and the other players where the client last saw them (at most 64 of each). A mover
// with a velocity runs on before each replayed command by the server's own push (shooter/push.hpp:
// carrying what stands on it, shoving what it moves into), as `advance_movers` does for the float
// move.
void original_prediction_place(OriginalPrediction& p, const ModelAt* movers, int mover_count,
                               const Vec3f* players, int player_count);

// Its own player: the acknowledged state with `count` later commands run through the original
// step on the stand-in. The returned state's origin, velocity and ground are the predicted ones;
// `each` (when given, `count` of them) the same after each command, in order. `tele` (when given):
// a teleporter the player's box reaches is taken as the server takes it (`skip` left out), the
// first such command in `hit` (shooter/teleport_ahead.hpp). `mover_at` (when given): the seconds
// after their frame the movers have run on to before each command, as for the float move.
MoveState predict_player_original(OriginalPrediction& p, const NetPlayer& acked,
                                  const MoveCmd* cmds, int count, MoveState* each = nullptr,
                                  const ge::shooter::Teleporters* tele = nullptr, int skip = -1,
                                  ge::shooter::TeleHit* hit = nullptr,
                                  const float* mover_at = nullptr);

}  // namespace ge::classic
