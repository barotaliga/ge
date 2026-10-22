// WHAT A SKIER LEAVES IN THE SNOW, as arithmetic: each frame, each ski on the snow lays a PRINT,
// a capsule from where it touched at the last frame to where it touches now, so a track is
// continuous at any speed and any frame rate. A game hands the prints to its trail map (the
// mountain's `mountain/app/trails.hpp` for Fall Line), which keeps them.
//
// TWO LINES, A STANCE APART. The skis run `kTrackStance` m either side of the skier's line, each
// a waist wide. What the turn does to them is read off the state, never drawn as a decal:
// - THE LOAD: the outside ski of a turn (the left on the right edges) carries more of the
//   skier's weight the harder he is edged, so its groove is the deeper.
// - THE CARVE: a ski on its edge cuts a narrower, deeper groove (a rail), and a flat ski a scuff.
// - THE SKID: a ski slid sideways (its way off the skier's travel) scrapes a band as wide as the
//   ski's footprint across the travel, shallower, and pushes a berm out beside it.
// THE SNOW decides the depth: on the groomed piste a scuff of about a centimetre; in loose snow
// the furrow a ski presses whatever the physics' sink (a hand deep), or the sink when deeper; the
// berm and the slump of its walls with it (soft round walls in powder, square on the piste).
//
// IN THE AIR there is no print, and the pen lifts (a landing starts a new line); a skier DOWN
// gouges one wide print where his body slides. A move further than `kTrackJump` m in a frame is a
// reset, not a track, and one under `kTrackStill` m none at all.
//
// ge:summary The prints a skier's skis lay in the snow each frame: two capsules, load, carve, skid
// ge:keywords skier tracks trail prints ski-print track-pen grooves furrow berm carve skid load
// ge:keywords outside-ski stance capsule ski_prints body-gouge
// ge:skill skier render
// ge:test tests/skier_tracks_test.cpp
#pragma once

#include "skier/skier.hpp"
#include "skier/skis.hpp"

namespace ge::skier {

// One print: a capsule from (ax, az) to (bx, bz) on the plan, `half_width` m either side of
// its axis, `depth` m deep at its axis, a berm `berm` m high just outside it, and walls that
// fall away as 1 - u^`wall` across it (u: 0 on the axis, 1 at the edge; 2 round, 10 square).
struct SkiPrint {
  double ax, az, bx, bz;
  double half_width;
  double depth;
  double berm;
  double wall;
};

// Each foot's way off the skier's line, m.
inline constexpr double kTrackStance = 0.11;
// A move further than this in a frame is a reset, not a track, m; one shorter than the second
// is no move (the pen waits for the skier to get somewhere).
inline constexpr double kTrackJump = 6.0;
inline constexpr double kTrackStill = 0.005;
// The furrow a ski presses into loose snow whatever its sink, m, and the scuff on the piste.
inline constexpr double kPowderFurrow = 0.12;
inline constexpr double kPackedScuff = 0.012;
// A body sliding on the snow: its print's half width and its gouge in loose snow, m.
inline constexpr double kBodyHalfWidth = 0.35;
inline constexpr double kBodyGouge = 0.14;

// Where each ski (and the body) last touched: a skier's pen, kept by the game between frames.
struct TrackPen {
  double x[2] = {0, 0}, z[2] = {0, 0};  // the left ski's, then the right's
  double body_x = 0, body_z = 0;
  bool skis = false;  // the skis' points are on the snow
  bool body = false;  // the body's point is
};

// The skis' (or the body's) prints from the pen to `s` now, into `out`: two prints, one (down),
// or none (in the air, the first frame, a reset). Moves the pen. `spec` sizes the skis.
int ski_prints(const SkierState& s, const SkiSpec& spec, TrackPen& pen, SkiPrint out[2]);

// What share of the skier's weight the ski on `side` (0 left, 1 right) carries at `edge` rad:
// a half each flat, the outside ski up to 0.85 of it on a full edge.
double ski_load(double edge, int side);

}  // namespace ge::skier
