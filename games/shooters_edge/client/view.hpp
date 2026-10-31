// WHAT THE PLAYER SEES OF THE LISTEN SERVER, read off its state after each frame and never
// written back: the HUD's status snapshot (hud/status.hpp) from the player's entity (health, the
// armour and its class, the four ammunitions, the weapons held and the one in hand, frags), and
// the first-person camera's subject (engine/camera/subject.hpp): the feet and the eyes over them
// from the mod's view offset, the velocity, on the ground or not, and the client's view angles.
//
// THE AXES: the server's are map units with z up and angles in degrees (yaw counter-clockwise
// from +x, pitch down positive); the engine's are metres with y up (brush/map.hpp's `to_engine`),
// a heading clockwise from +z and a pitch up positive. `engine_point`, `engine_heading` and
// `engine_pitch` are the only conversions.
//
// ge:summary The player's view of the listen server: the HUD's status and the camera's subject
// ge:keywords hud-status status_of camera-subject subject_of view-height view-ofs engine_point
// ge:keywords engine_heading engine_pitch weapons armour ammo items first-person
// ge:keywords powerups quad shield power_s
// ge:skill shooter camera ui
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include "camera/first_person.hpp"
#include "camera/subject.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/hud/status.hpp"

namespace shooters_edge::client {

// A map point (units, z up) in engine metres (y up).
ge::camera::Vec3 engine_point(sh::Vec3f p);
// A map yaw (degrees) as an engine heading, and a map pitch (degrees, down positive) as an engine
// pitch (rad, up positive).
double engine_heading(float yaw_deg);
double engine_pitch(float pitch_deg);

// The HUD's snapshot of the player at the client's time `t`.
hud::Status status_of(const Listen& l, double t);

// The camera's subject: the feet, the velocity, grounded, the look. `row`'s standing eye height
// is set from the mod's view offset (the eyes over the feet).
ge::camera::Subject subject_of(const Listen& l, ge::camera::FirstPersonRow& row);

}  // namespace shooters_edge::client
