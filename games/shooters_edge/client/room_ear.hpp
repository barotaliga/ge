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
// UNDER WATER (`s_underwater`, `s_underwater_gain_hf`): while the eye is in a liquid the whole mix
// is heard through the synth's muffle, its treble over 5 kHz cut to the setting's share; out of it
// the mix is dry again at once. One setting of the classic games', on in Quake II as its own client
// has it and off in Quake, which never muffled.
//
// ge:summary Room acoustics on a classic map: the listener's room probed by rays through the BSP's
// leaves, the synth's reverb glided to it
// ge:keywords ce_acoustics acoustics room-sound reverb echo room-probe bsp-acoustics listener
// ge:keywords room_ear hear_room bsp_ray stone-hall corridor open-sky s_underwater
// ge:keywords s_underwater_gain_hf underwater-muffle hear_under s_swapstereo hear_swap
// ge:skill audio shooter
// ge:test tests/shooters_edge_room_ear_test.cpp
#pragma once

#include "audio/room.hpp"
#include "audio/synth.hpp"
#include "brush/map.hpp"
#include "console/console.hpp"

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
  double next_s = 0;   // when the room is probed again (the frame clock)
  bool wet = false;    // the synth holds a room of ours
  float muffled = -1;  // the muffle's treble last given to the synth (-1: none yet)
};

// The muffle under water's settings.
struct UnderEar {
  bool on = false;       // s_underwater (Quake II's default 1)
  double treble = 0.25;  // s_underwater_gain_hf: the treble's share heard, 0.001..1
  bool swap = false;     // s_swapstereo: left and right swapped at the output
  int swap_sent = -1;    // what the synth was last told (-1: nothing yet)
};
inline constexpr float kUnderCornerHz = 5000;  // the muffle's corner

// s_underwater, s_underwater_gain_hf and s_swapstereo in the classic games' layer, bound to `s`.
void add_under_cvars(ge::console::Console& c, UnderEar& s);
// One frame: the synth muffled while `under` (the eye in a liquid) and `s.on`, dry otherwise; told
// only when that changes (and on the ear's first frame).
void hear_under(RoomEar& ear, ge::audio::Synth& synth, const UnderEar& s, bool under);
// One frame: the synth's channels swapped as s_swapstereo says, told only when that changes.
void hear_swap(ge::audio::Synth& synth, UnderEar& s);

// One frame: with `on` and a map, the room probed when its time comes and given to the synth;
// otherwise the synth's room set back to dry once.
void hear_room(RoomEar& ear, ge::audio::Synth& synth, const ge::brush::Map* map, bool on,
               const double eye[3], double heading, double now_s);

}  // namespace shooters_edge::client
