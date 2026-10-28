// THE ROOM ROUND THE LISTENER, probed from the level's own structure: a fixed fan of rays cast
// from the ear through a callback the game answers (its BSP, its brushes, its heightfield and
// houses), read as the numbers room acoustics is made of, then turned into the synth's
// `RoomSound` (reverb.hpp):
//
//   the MEAN FREE PATH   how far sound goes between two bounces: 4V / S for a diffuse room.
//                        Each ray's distance d stands for a cone of the sphere, so the room's
//                        volume is the mean of d^3 times 4 pi / 3 and its walls' area the mean
//                        of d^2 / cos(the ray's angle to the wall's normal) times 4 pi
//   the OPEN SHARE       the rays that met nothing within `reach_m`: sky, a far valley, a door
//                        out. Open sound never returns, so it absorbs as a perfect wall would
//   the ABSORPTION       the hit surfaces' mean (the callback reports each surface's own:
//                        stone low, carpet and snow high), mixed toward 1 by the open share
//   the DECAY (RT60)     Eyring's formula, 0.161 V / (-S ln(1 - a)) s, which is 0.161 (mfp / 4)
//                        / -ln(1 - a): small hard rooms ring briefly, big stone halls for
//                        seconds, a courtyard hardly at all
//   the TAIL'S LEVEL     the reverberant field against a source `source_m` away: the room's
//                        critical distance is 0.057 sqrt(V / RT60) m (where the direct sound
//                        and the reverberation are equally loud), so the tail's amplitude is
//                        source_m over it, times the closed share squared (sound that leaves
//                        builds no field: a court keeps half its tail, an open field none)
//   the EARLY ECHOES     the nearest hits (one a sector of the sphere round the ear, so they
//                        come from all sides), each the way to the wall and back at the speed
//                        of sound, its gain the spreading (1 / the path) times what the surface
//                        returns, its side the wall's direction against the listener's right
//   the OUTDOOR ECHO     the synth's own (EchoRoom), kept by the open share and faded as the
//                        walls close in
//
// DETERMINISTIC: the directions are fixed (a Fibonacci sphere, made once through core/libm), so
// the same world answers the same probe with the same numbers on every platform; the probe
// allocates nothing and never draws a random number. A game probes every few frames and glides
// the synth between the answers.
//
// ge:summary The room round the listener probed by rays: mean free path, open share, RT60,
// early echoes, the synth's RoomSound
// ge:keywords audio room room-acoustics probe rays reverb rt60 eyring sabine mean-free-path
// ge:keywords absorption open-share critical-distance early-reflections fibonacci-sphere
// ge:keywords probe_room room_sound ray-callback bsp-acoustics level-structure
// ge:skill audio
// ge:test tests/audio_room_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "audio/reverb.hpp"

namespace ge::audio {

inline constexpr int kProbeRays = 64;

// What a ray met: a surface `distance_m` away that takes `absorption` (0..1) of the sound that
// strikes it, facing `normal` (unit; zero when the world does not say, taken as facing the ray);
// `hit` false when nothing was met within the reach.
struct RayHit {
  bool hit = false;
  double distance_m = 0;
  double absorption = 0.05;
  double normal[3] = {0, 0, 0};
};

// The game's world, as rays: from the ear along a unit direction, at most `reach_m`. World axes
// are the engine's (x right, y up, z forward on heading 0).
struct RoomWorld {
  const void* ctx = nullptr;
  RayHit (*ray)(const void* ctx, const double from[3], const double dir[3],
                double reach_m) = nullptr;
};

struct ProbeSettings {
  double reach_m = 120;  // a ray that meets nothing by here is open
  double source_m = 6;   // the typical source's distance the tail's level is set for
  double open_echo = 1;  // the outdoor echo's share under an open sky (RoomSound::echo)
};

struct RoomProbe {
  int rays = 0;
  int hits = 0;
  double open_share = 0;  // the rays that met nothing
  double mean_free_path_m = 0;
  double volume_m3 = 0;  // of the enclosed part, from the hits
  double area_m2 = 0;
  double absorption = 0;            // the hit surfaces' mean
  double effective_absorption = 0;  // with the open share mixed in
  double rt60_s = 0;
  double critical_m = 0;  // the critical distance
  // The early echoes' walls: their distance, the share they return, their unit direction.
  int walls = 0;
  std::array<double, kEarlyTaps> wall_m{};
  std::array<double, kEarlyTaps> wall_returns{};
  std::array<std::array<double, 3>, kEarlyTaps> wall_dir{};
  // Every ray's answer, for a lab's drawing.
  std::array<RayHit, kProbeRays> ray{};
};

// The fixed fan: direction `i` of kProbeRays, unit, spread evenly over the sphere.
const std::array<double, 3>& probe_direction(int i);

// Cast the fan from `ear` into `world`.
RoomProbe probe_room(const RoomWorld& world, const double ear[3],
                     const ProbeSettings& settings = {});

// The probe as the synth's room, for a listener whose right is `right` (unit, world axes).
RoomSound room_sound(const RoomProbe& probe, const double right[3],
                     const ProbeSettings& settings = {});

}  // namespace ge::audio
