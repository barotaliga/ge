// WHO WARPED THIS FRAME: the players the window sees arrive (a spawn into the match, a respawn, a
// teleport's far end) and leave (a teleport's near end), read off where each player's entity is
// frame to frame, for the effects (world_fx.hpp: the sparks, the flash, the figure's dissolve).
// Presentation only, the same for a match and a campaign, with nothing asked of the mod.
//
// - ARRIVED: an entity that was not there last frame (it joined, or the map began), or one that
//   moved farther in a frame than anything runs or flies (kJumpUnits, and kJumpUnitsPerS a second
//   of the frame), at its new place.
// - LEFT: the same jump by a body that was alive at its old place, there: a dead body's jump is a
//   respawn, and the corpse the mod leaves behind stays where it fell.
// - SHOWN: how whole a figure is through its warp, eased: 0 to 1 over kArriveS arriving, 1 to 0
//   over kLeaveS leaving (render/morph.hpp's MorphWarp::shown).
//
// ge:summary The players who arrived or left this frame (spawn, respawn, teleport) and how shown
// ge:keywords warps warp watch_warps warp_shown arrival departure spawn respawn teleport jump
// ge:keywords materialize dematerialize spawn-effect teleport-effect arrival_time
// ge:skill render shooter
// ge:test tests/shooters_edge_warps_test.cpp
#pragma once

#include <span>
#include <unordered_map>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

// A player's entity this frame as the server holds it.
struct Body {
  int entity = 0;
  sh::Vec3f origin;  // map units
  bool alive = false;
};

struct Warp {
  int entity = 0;
  sh::Vec3f at;   // map units: where it arrived, or where it left from
  double t0 = 0;  // s: when it began
  bool arriving = true;
};

inline constexpr double kArriveS = 0.7, kLeaveS = 0.8;
inline constexpr float kJumpUnits = 96, kJumpUnitsPerS = 1500;

struct WarpWatch {
  std::unordered_map<int, Body> last;  // by entity: the bodies last frame
  std::vector<Warp> live;              // the warps still running
};

// This frame's bodies at `t` s, `dt` s after the last frame: the warps that began appended to
// `began` (and kept in `w.live` until they end).
void watch_warps(WarpWatch& w, std::span<const Body> now, double t, double dt,
                 std::vector<Warp>& began);
// How whole the figure is at `t` through `warp`, 0..1.
double warp_shown(const Warp& warp, double t);
// The share `entity` arriving is shown at `t` (1: no arrival running).
double arrival_shown(const WarpWatch& w, int entity, double t);
// The seconds into an arrival at which it is shown `share` (the dust's landing times).
double arrival_time(double share);

}  // namespace shooters_edge::client
