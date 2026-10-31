// WHAT A CLIENT IS SENT OF ITS OWN PLAYER EACH FRAME on a single-player program's server
// (porting/shooters-edge/campaign.md § The client data), beside the statistics: the blow it took
// and the pitch its view drifts to. Run at the end of every frame (`qc_frame`), for every active
// player, only on the single-player form (the one whose layout carries `idealpitch`): a
// multiplayer program keeps its own damage fields and the server never touches them.
//
// - THE BLOW: when the player's `dmg_take` or `dmg_save` is non-zero, the frame's record
//   (`ClientInfo::blow`) is the two (the blood and the armour, each a byte: 0..255) and the source
//   point, the `dmg_inflictor`'s box centre (its origin and the middle of its box); then both
//   fields are cleared, so the next blow starts from nothing. A frame without one has
//   `blow.hit` false.
// - THE IDEAL PITCH (`sv_idealpitchscale`, default 0.8), only on the ground: six points 36, 48 ...
//   96 units ahead along the body's yaw, each traced 160 units down from the eye (the world and
//   the brush models: no monsters). Any trace that starts in a solid or finds no floor leaves the
//   ideal as it was. The floor's height step from one point to the next: a step within 0.1 of
//   flat is skipped; the others must all be one slope (within 0.1 of each other) or the ideal is
//   left; no slope at all sets 0; fewer than two steps of it leave the ideal; else the ideal is
//   minus the step times the scale (degrees: down a slope looks down). Written to the player's
//   `idealpitch` field, where the client reads it.
//
// The punch (`punchangle`, the view's recoil kick the program sets when firing) needs nothing
// here: the loop decays it (framemove.hpp) and the client reads the field.
//
// ge:summary A single-player client's own data each frame: the blow taken and the ideal pitch
// ge:keywords client-data damage dmg_take dmg_save dmg_inflictor blow idealpitch ideal-pitch
// ge:keywords sv_idealpitchscale slope lookspring campaign take_client_data
// ge:skill shooter progs
// ge:test tests/shooters_edge_client_data_test.cpp
#pragma once

#include "shooter/server.hpp"

namespace shooters_edge::server {

struct QcServer;

inline constexpr float kIdealPitchScale = 0.8f;  // sv_idealpitchscale's default

// The ideal pitch of player `e` from his place and yaw, or `current` when the floor ahead is not
// one slope (see above); `scale` is sv_idealpitchscale.
float ideal_pitch(const ge::shooter::Server& sv, int e, float current, float scale);

// The frame's client data for every active player (the blow taken and cleared, the ideal pitch).
void take_client_data(QcServer& s);

}  // namespace shooters_edge::server
