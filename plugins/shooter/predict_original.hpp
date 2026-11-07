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
// ge:skill shooter net
// ge:test tests/shooter_predict_original_test.cpp
#pragma once

#include <vector>

#include "shooter/predict.hpp"
#include "shooter/server.hpp"

namespace ge::shooter {

// A brush model by its index among the level's models (`*k`: 0 the world), where it stands.
struct ModelAt {
  int model = 0;
  Vec3f origin;
};

struct OriginalPrediction {
  Server sv;
  int placed = 0;  // entities after the player in use: movers, then players
};

// The stand-in for a level: `models[k]` is `*k` (0 the world); `set` the server's settings as the
// client knows them (the move's, the original move's own, maxvelocity, who jumps).
void original_prediction_level(OriginalPrediction& p, const std::vector<BrushModel>& models,
                               const ServerSettings& set);

// The movers and the other players where the client last saw them (at most 64 of each).
void original_prediction_place(OriginalPrediction& p, const ModelAt* movers, int mover_count,
                               const Vec3f* players, int player_count);

// Its own player: the acknowledged state with `count` later commands run through the original
// step on the stand-in. The returned state's origin, velocity and ground are the predicted ones.
MoveState predict_player_original(OriginalPrediction& p, const NetPlayer& acked,
                                  const MoveCmd* cmds, int count);

}  // namespace ge::shooter
