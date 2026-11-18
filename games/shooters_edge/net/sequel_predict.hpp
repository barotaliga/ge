// THE PLAYER'S OWN MOVES RUN AHEAD OF A SERVER OF THE SEQUEL (sequel_remote.hpp): from the newest
// state the server sent of the player (the frame's packed origin, velocity, flags, timer, gravity
// and delta angles), every move sent since the one the server last acknowledged is run through the
// sequel's own player move (plugins/classic/sequel_move.hpp), over the level's brushes and the
// movers where the newest frame put them, so the player moves the frame his hands do, not a round
// trip later. The move is the one the server runs: the same integer state, the same command (the
// jump as the up move), the air acceleration the server states (configstring `air_accelerate`).
//
// A frame that proves the prediction wrong by 4 to 64 units is not snapped but folded into a
// correction that decays (plugins/shooter/predict.hpp's `Smoothing`): the newest move is predicted
// from the old base and from the new one, and the difference is the correction. A frame with the
// no-prediction flag (a teleport's hold, a pause) or more moves unacknowledged than the ring keeps
// draws the server's place. Not yet: the other players' boxes, a mover's turn, the view height
// while ducking ahead of the server (the server's is drawn).
//
// ge:summary The player's own moves predicted ahead of a server of the sequel, over its brushes
// ge:keywords sequel-prediction predict sequel_move movers smoothing correction acknowledged
// ge:keywords sequel_predict_map sequel_predict_step cl_nopred
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#pragma once

#include <cstdint>
#include <memory>

#include "brush/trace.hpp"
#include "classic/sequel_move.hpp"
#include "shooter/predict.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::net {

struct SequelClientEnd;

inline constexpr int kSqSentRing = 64;  // the moves kept by outgoing sequence

struct SequelPredict {
  const ge::brush::Map* map = nullptr;  // the level's brushes (the window's loaded map)
  std::unique_ptr<ge::brush::BrushTracer> tracer;
  ge::shooter::Smoothing smooth;
  // The base the last prediction ran from: the server's state and the move it acknowledged.
  ge::classic::SequelState base;
  uint32_t base_acked = 0;
  int64_t base_frames = -1;  // the client's frame count it was taken at
  bool have_base = false;
  ge::shooter::Vec3f origin;    // the newest prediction, the correction not added
  ge::shooter::Vec3f velocity;  // its velocity, units/s
  bool on_ground = false;       // it stands on something
  ge::shooter::Vec3f eye_up;    // the newest frame's eye over the server's place of the player
  int replayed = 0;             // the moves run past the acknowledged one, the last step
  bool predicted = false;       // the last step drew a prediction
};

// The level the prediction moves through (nullptr: none, nothing predicted).
void sequel_predict_map(SequelPredict& p, const ge::brush::Map* map);
// One window frame: the player's place run ahead of the newest frame read (`d`, the client's
// recording) by the moves `c` sent since; `dt` decays the correction. True with `p.origin` (and
// the drawn place `smoothed`) when it predicted.
bool sequel_predict_step(SequelPredict& p, const SequelClientEnd& c, const client::Demo& d,
                         double dt, ge::shooter::Vec3f& smoothed);

}  // namespace shooters_edge::net
