// THE PLAYER'S OWN LIGHTNING BEAM, TRACED AGAIN EVERY FRAME ALONG HIS AIM (cl_fakeshaft; drawn by
// world_fx.hpp's beams). The server sends the player's own lightning beam
// only as often as the gun fires (ten times a second) and a round trip late, so drawn as sent it
// jumps and the line it burns is a few straight chords. Instead, every frame it is traced again
// from the eye (the drawn, predicted one) along a direction `share` of the way from the server's
// line to the view's: through the map's world and the living bodies' boxes (`bodies`, the player's
// own left out), `kOwnBeamReachU` long, rooted `kOwnBeamDropU` under the eye as the server roots
// it. Its end, normal and the classic bolt's segments follow; so do its burns and lights. Out:
// the root and end (map units) and the normal at the end (map, toward the root); false when there
// is no map to trace.
//
// ge:summary The own lightning beam traced every frame along the aim (cl_fakeshaft), no GPU
// ge:keywords own_beam fakeshaft cl_fakeshaft lightning beam shaft aim trace burn-line marks
// ge:skill render shooter
// ge:test tests/shooters_edge_own_beam_test.cpp
#pragma once

#include <span>

#include "shooter/hull.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/warps.hpp"

namespace shooters_edge::client {

inline constexpr float kOwnBeamReachU = 600, kOwnBeamDropU = 6;
// One beam's line traced from `from` along `dir` (any length): the map's world and the living
// bodies' boxes (the player `own`'s left out), `kOwnBeamReachU` long. Out: where it stops and the
// normal there (toward `from`; the line's own way back where nothing is in reach). False without a
// map.
bool beam_trace(const LoadedMap* map, const sh::Vec3f& from, const sh::Vec3f& dir,
                std::span<const Body> bodies, int own, sh::Vec3f& to, sh::Vec3f& normal);
// THE EDGE A SWEEP CROSSES: a beam that landed along `was_dir` last frame and along `dir` now, on
// two different faces (a wall and the floor, the two sides of a pillar, a ledge's top), crossed
// the edge between them. Its burnt line is laid up to that edge and starts again just past it, so
// the line goes round the corner unbroken instead of stopping short on one face and starting late
// on the next. Found by halving the turn (`kEdgeSteps` at most) until the two hits are within
// `kEdgeU`. Out: 2 and the last point on the old face, then the first on the new (each with its
// normal); 0 on one face, or with no map.
inline constexpr int kEdgeSteps = 12;
inline constexpr float kEdgeU = 0.5f, kSameFaceDot = 0.99f, kSameFaceU = 1;
int beam_edge(const LoadedMap* map, const sh::Vec3f& from, const sh::Vec3f& was_dir,
              const sh::Vec3f& dir, std::span<const Body> bodies, int own, sh::Vec3f at[2],
              sh::Vec3f n[2]);
inline constexpr float kBodyHalfU = 16, kBodyLowU = 24, kBodyHighU = 32;  // a player's box
bool own_beam(const LoadedMap* map, const sh::Vec3f& eye, const sh::Vec3f& view_dir,
              const sh::Vec3f& server_from, const sh::Vec3f& server_to, double share,
              std::span<const Body> bodies, int own, sh::Vec3f& from, sh::Vec3f& to,
              sh::Vec3f& normal);

}  // namespace shooters_edge::client
