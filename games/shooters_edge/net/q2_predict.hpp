// THE PLAYER'S OWN MOVES RUN AHEAD OF A SERVER OF QUAKE II (q2_remote.hpp): from the newest
// state the server sent of the player (the frame's packed origin, velocity, flags, timer, gravity
// and delta angles), every move sent since the one the server last acknowledged is run through the
// Quake II's own player move (plugins/classic/q2_move.hpp), over the level's brushes and the
// movers where the newest frame put them, so the player moves the frame his hands do, not a round
// trip later. The move is the one the server runs: the same integer state, the same command (the
// jump as the up move), the air acceleration the server states (configstring `air_accelerate`),
// and on our server the whole feel it runs the move on (svc 122: `q2_feel_of`).
//
// A frame that proves the prediction wrong by 4 to 64 units is not snapped but folded into a
// correction that decays (plugins/shooter/predict.hpp's `Smoothing`): the newest move is predicted
// from the old base and from the new one, and the difference is the correction. A frame with the
// no-prediction flag (a teleport's hold, a pause) or more moves unacknowledged than the ring keeps
// draws the server's place.
//
// THE OTHER PLAYERS (`q2_predict_others`), on our server, which sends after each frame every
// other player's state and the move it last ran of his, with the milliseconds since (svc 124, to a
// client that asked: net/q2_net_server.hpp): each is run forward by the QuakeWorld join's rule
// (`other_msec`, plugins/shooter/predict.hpp: from the state's time to the window's time less the
// measured round trip plus 20 ms, at most 255 ms) with that move through Quake II's move, so he
// is drawn where he is now, not where the last 10 Hz frame had him. `cl_predict_players 0` draws
// the frames' places.
//
// A STOCK SERVER sends no moves, only its 10 Hz frames, so its players are run on
// (`q2_run_on_others`) from the newest frame at the speed the last two frames show, to the
// server's present as the window reckons it (the frame's arrival less half the measured round
// trip, run to now: at most 100 ms), the box traced against the
// brushes so none is drawn through a wall: a runner is drawn near where he is, not a frame and a
// round trip behind. A player who moved more than 200 units between the frames (a teleport, a
// respawn) or stood still is drawn as the frames have him.
//
// THE EYE'S HEIGHT (`cl_predict_crouch`): the predicted move's standing or ducked eye replaces the
// frame's, so ducking and standing up move the view the frame the key goes down, not a round trip
// later. 1 (the default) on our server, whose move the client is told; 2 on a stock server too.
//
// MOVERS RUN ON (`cl_predict_movers`): each door, plat and train runs on through the replay at the
// speed the last two frames show it going (by their server times), stopping where it was seen
// standing (plugins/shooter/predict.hpp, `MoverTrack`), and carries the player who stands on it or
// shoves him where it moves into him; so a lift ridden or a door run through is where the server
// will have it when it runs the move; the window draws them there too (`q2_draw_movers_ahead`),
// so the lift is under his feet. The server moves them once a frame (100 ms), so they run on in
// whole frames as the moves pass the frame's time. A new frame's correction predicts the newest
// move from the old base over the movers as they were then.
//
// Not yet: the other players' boxes in the player's own prediction, a mover's turn.
//
// ge:summary The player's own moves predicted ahead of a server of Quake II, over its brushes
// ge:keywords q2-prediction predict q2_move movers smoothing correction acknowledged
// ge:keywords q2_predict_map q2_predict_step cl_nopred q2_predict_others
// ge:keywords other-players other_msec cl_predict_players other-moves q2_run_on_others
// ge:keywords extrapolate stock-server run-on cl_predict_crouch duck-view eye-height
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "brush/trace.hpp"
#include "classic/q2_move.hpp"
#include "shooter/predict.hpp"
#include "shooter/teleport_ahead.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::net {

struct Q2ClientEnd;

inline constexpr int kSqSentRing = 64;  // the moves kept by outgoing sequence

// A brush model the prediction runs over: where the newest frame put it, how fast it goes and how
// far it runs on before it stops (plugins/shooter/predict.hpp, `mover_motion`).
struct Q2Mover {
  uint32_t model = 0;
  ge::shooter::Vec3f at, velocity;
  float reach = 1e30f;
};

struct Q2Predict {
  const ge::brush::Map* map = nullptr;  // the level's brushes (the window's loaded map)
  std::unique_ptr<ge::brush::BrushTracer> tracer;
  ge::shooter::Smoothing smooth;
  // The base the last prediction ran from: the server's state and the move it acknowledged.
  ge::classic::Q2State base;
  uint32_t base_acked = 0;
  int64_t base_frames = -1;  // the client's frame count it was taken at
  bool have_base = false;
  ge::shooter::Vec3f origin;    // the newest prediction, the correction not added
  ge::shooter::Vec3f velocity;  // its velocity, units/s
  bool on_ground = false;       // it stands on something
  int water_level = 0;          // how deep it stands in water (0 dry .. 3 under)
  ge::shooter::Vec3f eye_up;    // the newest frame's eye over the server's place of the player
  // cl_predict_crouch: the eye's height predicted too, so a duck lowers the view at once. 0 the
  // server's; 1 on our server, which tells the move it runs (svc 122); 2 on any server.
  int crouch = 1;
  float eye_shift = 0;     // the predicted eye's height less the newest frame's
  int replayed = 0;        // the moves run past the acknowledged one, the last step
  bool predicted = false;  // the last step drew a prediction
  // A miss: a new frame put the newest move somewhere else than the last base did (the
  // correction's size). `cl_showmiss` (`show_misses`) prints each one as the frame it came in on.
  bool show_misses = false;
  // cl_predict_teleport: the level's teleporters (classic/teleporters.hpp) taken in the
  // prediction, so a teleport is drawn and the view turned the move it happens; the guess the
  // server has not yet answered.
  bool teleports = true;
  bool pushes = true;                    // cl_predict_push: the push triggers taken the same way
  bool movers = true;                    // cl_predict_movers: the movers run on through the replay
  ge::shooter::MoverTrack movers_track;  // their motion over the frames
  int64_t track_frames = -1;             // the client's frame count it was taken at
  std::vector<Q2Mover> movers_last;      // the movers the last prediction ran over
  float movers_ahead_s = 0;              // how far on the newest move ran them (drawn there)
  ge::shooter::TeleTables tele;          // the teleporters, and the push triggers after them
  ge::shooter::TeleGuess guess;
  bool teleported = false;       // the last step's moves went through one
  int16_t delta_angles[3] = {};  // the last step's predicted view turn (the server's own)
  int64_t misses = 0;            // since the client began predicting
  int64_t misses_told = 0;       // the ones printed (or passed over while `show_misses` was off)
  float miss_units = 0;          // the last one's size, units
  int miss_frame = 0;            // the frame it came in on
};

// The feel the move is predicted on: the one our server told (svc 122), else the stock move with
// the server's stated air acceleration.
ge::classic::Q2Feel q2_feel_of(const client::Demo& d);
// The movers in `st` drawn where the last prediction ran them (`cl_predict_movers`).
void q2_draw_movers_ahead(const Q2Predict& p, client::DemoState& st);
// The level the prediction moves through (nullptr: none, nothing predicted).
void q2_predict_map(Q2Predict& p, const ge::brush::Map* map);
// One window frame: the player's place run ahead of the newest frame read (`d`, the client's
// recording) by the moves `c` sent since; `dt` decays the correction. True with `p.origin` (and
// the drawn place `smoothed`) when it predicted.
bool q2_predict_step(Q2Predict& p, const Q2ClientEnd& c, const client::Demo& d, double dt,
                     ge::shooter::Vec3f& smoothed);
// The other players `c` has moves of, run forward to the network's time `now_s`, drawn in `d`'s
// players; the count, and the most milliseconds one was run in `most_msec`.
int q2_predict_others(Q2Predict& p, const Q2ClientEnd& c, client::Demo& d, double now_s,
                      int* most_msec);

inline constexpr int kSqRunOnMostMs = 100;         // a stock server's players run on at most this
inline constexpr float kSqRunOnMostMove = 200.0f;  // units between two frames: further snaps
// A stock server's other players run on from the newest frame (`now`, read at the network's time
// `frame_s`) at the speed since the one before (`old`, `gap_s` earlier), to `now_s`; the count.
int q2_run_on_others(const Q2Predict& p, const Q2ClientEnd& c, client::Demo& d,
                     const std::vector<client::DemoEntity>& old,
                     const std::vector<client::DemoEntity>& now, double frame_s, double gap_s,
                     double now_s, int* most_msec);

}  // namespace shooters_edge::net
