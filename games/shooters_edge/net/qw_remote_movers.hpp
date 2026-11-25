// THE MOVERS AND THE DRAWN ENTITIES OF A SERVER JOINED (qw_remote.hpp): the brush models the
// prediction runs over and where the window draws them.
//
// - `remote_movers`: the doors, plats and trains where the newest frame put them (never the drawn
//   places, which are between the last two frames), with their speed and their stops when
//   `cl_predict_movers` is on (shooter/predict.hpp, `MoverTrack`).
// - `remote_mover_times`: when the server will run each replayed command, as seconds after the
//   frame the movers were seen in: the acknowledged command ran as that frame left, each later one
//   as much later as it was sent (each command's time added up when the acknowledged one's send
//   is no longer known, then null).
// - `remote_movers_keep` (the main prediction): the movers it ran over and how far on its newest
//   move ran them (they are drawn there); `remote_movers_turn` (each step, before it): the last
//   step's kept for a new frame's correction, which predicts the newest move over them.
// - `remote_lerp_entities`: every entity drawn between the last two frames read, at the drawn time
//   (`cl_nolerp 0`); one that moved more than `kLerpMost` snaps.
// - `remote_draw_movers_ahead`: then the movers drawn where the player's own prediction ran them
//   (`cl_predict_movers`), so a lift he rides is under his feet and a door he runs through is as
//   open as his move found it.
//
// ge:summary The movers a server joined predicts over and draws, and the entities drawn between
// ge:summary frames
// ge:keywords remote_movers remote_lerp_entities remote_draw_movers_ahead cl_predict_movers
// ge:keywords movers lerp cl_nolerp mover-ahead
// ge:skill net shooter
// ge:test tests/shooters_edge_remote_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/predict.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::net {

std::vector<ge::shooter::ClientModel> remote_movers(const Remote& r);
const float* remote_mover_times(const Remote& r, uint32_t acked, const uint32_t* seqs,
                                const ge::shooter::MoveCmd* cmds, int count, float* mover_at);
void remote_movers_keep(Remote& r, const std::vector<ge::shooter::ClientModel>& movers,
                        const float* mover_at, int count);
void remote_movers_turn(Remote& r);
void remote_lerp_entities(Remote& r, double now_s);
void remote_draw_movers_ahead(Remote& r);

}  // namespace shooters_edge::net
