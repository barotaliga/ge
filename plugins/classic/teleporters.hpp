// THE CLASSIC GAMES' TELEPORTERS AS A CLIENT FORESEES THEM (shooter/teleport_ahead.hpp): read off
// the level's own entities, so a client predicts a teleport on any server that runs the game's
// own rules.
//
// QUAKE (`q1_teleporters`): a `trigger_teleport` (its brush model's box, grown a unit a side as
// the server links it) sends the player to the first entity whose `targetname` is its `target`,
// when that is an `info_teleport_destination`: put 27 units over the marker, turned to its angles
// (`angles`, or `angle` as the yaw), 300 units/s along the way it faces (its pitch included), off
// the ground. Left out, as the client cannot know them: a teleporter with a `targetname` (it works
// only just after it is used) and one whose destination is anything else (a destination that
// rides a path).
//
// QUAKE II (`q2_teleporters`): a `misc_teleporter` with a `target` touches the player in a 16
// unit box over its disc ((−8, −8, 8) to (8, 8, 24) about it, grown a unit a side), and sends him
// to the first entity whose `targetname` is its `target` (a `misc_teleporter_dest`, an
// `info_notnull`): put 10 units over it, still, held 160 ms, his view turned to its angles
// (`q2_teleport_state`: the move's delta angles from the command that touched it).
//
// ge:summary The classic games' teleporters as a client foresees them, read off the level
// ge:keywords teleporters trigger_teleport info_teleport_destination misc_teleporter
// ge:keywords misc_teleporter_dest q1_teleporters q2_teleporters q2_teleport_state
// ge:keywords predict-teleport teleport-prediction
// ge:skill classic shooter net
// ge:test tests/classic_teleporters_test.cpp
#pragma once

#include "classic/q2_move.hpp"
#include "shooter/teleport_ahead.hpp"

namespace ge::brush {
struct Map;
}

namespace ge::classic {

inline constexpr float kQ1TeleportRise = 27;    // the destination's origin over its marker
inline constexpr float kQ1TeleportSpeed = 300;  // units/s along the destination's facing
inline constexpr float kQ2TeleportRise = 10;    // the player's origin over the destination
inline constexpr int kQ2TeleportHoldMs = 160;   // the move's teleport hold

ge::shooter::Teleporters q1_teleporters(const ge::brush::Map& map);
ge::shooter::Teleporters q2_teleporters(const ge::brush::Map& map);
// The teleporters of whichever game the map is (version 38: Quake II's; else Quake's).
ge::shooter::Teleporters level_teleporters(const ge::brush::Map& map);

// Quake II's arrival on its packed state: there, still, held, the view turned to the teleporter's
// angles from those of the command that touched it (`cmd_angles`, 65536ths of a turn).
void q2_teleport_state(Q2State& s, const ge::shooter::Teleporter& t, const int16_t cmd_angles[3]);

}  // namespace ge::classic
