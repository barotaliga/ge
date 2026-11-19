// THE ROOM ROUND THE EAR (ce_acoustics): the map's own structure probed from the listener every
// kProbeEveryS seconds through engine/audio's room probe (audio/room.hpp: a fixed fan of rays, the
// mean free path, the open share, the decay), and the synth's room glided to the answer, so a shot
// rings in a stone hall, is short in a corridor and dies away under an open sky. Either classic
// family: a ray walks the map's own leaves (brush::cell_at) in small paces and finds the wall by
// halving its last pace; sky, and nothing within reach, is open. Off (or with no map): the dry
// room, the sound as the pack has it.
//
// DETERMINISTIC and allocation free: the probe's directions are fixed and the walk is the map's
// tree; it changes only what is heard, never the game.
//
// ge:summary Room acoustics on a classic map: the listener's room probed by rays through the BSP's
// leaves, the synth's reverb glided to it
// ge:keywords ce_acoustics acoustics room-sound reverb echo room-probe bsp-acoustics listener
// ge:keywords room_ear hear_room bsp_ray stone-hall corridor open-sky
// ge:skill audio shooter
// ge:test tests/shooters_edge_room_ear_test.cpp
#pragma once

#include "audio/room.hpp"
#include "audio/synth.hpp"
#include "brush/map.hpp"

namespace shooters_edge::client {

inline constexpr double kProbeEveryS = 0.25;  // how often the room is probed again
inline constexpr double kRoomGlideS = 0.4;    // the synth's glide to each answer

// What a ray from `from` (engine metres: x right, y up, z forward) along the unit `dir` meets in
// `map` within `reach_m`: a wall's distance, or nothing (sky counts as nothing: sound that leaves
// never comes back).
ge::audio::RayHit bsp_ray(const ge::brush::Map& map, const double from[3], const double dir[3],
                          double reach_m);

// The listener's room in `map` heard from `ear` facing `right` (unit), as the synth's room.
ge::audio::RoomSound bsp_room(const ge::brush::Map& map, const double ear[3],
                              const double right[3]);

struct RoomEar {
  double next_s = 0;  // when the room is probed again (the frame clock)
  bool wet = false;   // the synth holds a room of ours
};

// One frame: with `on` and a map, the room probed when its time comes and given to the synth;
// otherwise the synth's room set back to dry once.
void hear_room(RoomEar& ear, ge::audio::Synth& synth, const ge::brush::Map* map, bool on,
               const double eye[3], double heading, double now_s);

}  // namespace shooters_edge::client
