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
// draws the server's place.
//
// THE OTHER PLAYERS (`sequel_predict_others`), on our server, which sends after each frame every
// other player's state and the move it last ran of his, with the milliseconds since (svc 124, to a
// client that asked: net/sequel_net_server.hpp): each is run forward by the classic join's rule
// (`other_msec`, plugins/shooter/predict.hpp: from the state's time to the window's time less the
// measured round trip plus 20 ms, at most 255 ms) with that move through the sequel's move, so he
// is drawn where he is now, not where the last 10 Hz frame had him. `cl_predict_players 0` draws
// the frames' places.
//
// A STOCK SERVER sends no moves, only its 10 Hz frames, so its players are run on
// (`sequel_run_on_others`) from the newest frame at the speed the last two frames show, to the
// server's present as the window reckons it (the frame's arrival less half the measured round
// trip, run to now: at most 100 ms), the box traced against the
// brushes so none is drawn through a wall: a runner is drawn near where he is, not a frame and a
// round trip behind. A player who moved more than 200 units between the frames (a teleport, a
// respawn) or stood still is drawn as the frames have him.
//
// Not yet: the other players' boxes in the player's own prediction, a mover's turn, the view
// height while ducking ahead of the server (the server's is drawn).
//
// ge:summary The player's own moves predicted ahead of a server of the sequel, over its brushes
// ge:keywords sequel-prediction predict sequel_move movers smoothing correction acknowledged
// ge:keywords sequel_predict_map sequel_predict_step cl_nopred sequel_predict_others
// ge:keywords other-players other_msec cl_predict_players other-moves sequel_run_on_others
// ge:keywords extrapolate stock-server run-on
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

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
  int water_level = 0;          // how deep it stands in water (0 dry .. 3 under)
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
// The other players `c` has moves of, run forward to the network's time `now_s`, drawn in `d`'s
// players; the count, and the most milliseconds one was run in `most_msec`.
int sequel_predict_others(SequelPredict& p, const SequelClientEnd& c, client::Demo& d, double now_s,
                          int* most_msec);

inline constexpr int kSqRunOnMostMs = 100;         // a stock server's players run on at most this
inline constexpr float kSqRunOnMostMove = 200.0f;  // units between two frames: further snaps
// A stock server's other players run on from the newest frame (`now`, read at the network's time
// `frame_s`) at the speed since the one before (`old`, `gap_s` earlier), to `now_s`; the count.
int sequel_run_on_others(const SequelPredict& p, const SequelClientEnd& c, client::Demo& d,
                         const std::vector<client::DemoEntity>& old,
                         const std::vector<client::DemoEntity>& now, double frame_s, double gap_s,
                         double now_s, int* most_msec);

}  // namespace shooters_edge::net
