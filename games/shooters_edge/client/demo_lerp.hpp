// A RECORDED GAME DRAWN BETWEEN ITS FRAMES, so a played demo is smooth whatever the rate it was
// recorded at and the rate it is drawn at (a 60 Hz display, a 30 fps video). Without it each
// picture shows the newest frame read: a recording's 77 frames a second fall unevenly on 60 or 30
// pictures, and a player whose packets reached the server unevenly jumps from frame to frame.
//
// ONE FRAME BEHIND: the playback reads a frame once its clock has passed the frame before it
// (timedemo.hpp's `playdemo_step`, demo_video.hpp's frame clock), so it always holds two: `from`
// (the frame before the last read, time t0) and `to` (the last, t1), the clock between them. A
// picture shows the demo at `f = (clock - t0) / (t1 - t0)`, held to 0..1.
//
// THE RULES (`lerp_player`, `lerp_entity`):
//
// | What | Drawn between | Drawn at `to` when |
// | --- | --- | --- |
// | a player (client, multiview) | origin straight, angles the shortest way round | missing from
// either frame; moved more than `kPlayerSnapUnits`; a corpse come alive (a respawn); gibbed in `to`
// and not a corpse in `from`; his view set by the server in `to` (the angles alone) | | an entity
// (every format) | origin straight, angles the shortest way round | missing from either frame;
// another model; moved more than `kEntitySnapUnits` | | the view | the eye rides the tracked
// entity's in-between origin; the angles turn the shortest way | the level's intermission; another
// player tracked; the view set by the server in `to` (angles) | | the clock | | the two frames more
// than `kMostLerpGap` apart (a stall: `to` as read), a level's start, a seek |
//
// A STRIDE: a thing that is not a player and stood still through the frame before (a walking
// monster, which the program moves a stride at a time, ten a second, while a recording holds 72
// frames a second) is drawn over the time since its last move instead of between two frames 14 ms
// apart (`DemoLerp::strides`, r_lerpmove): from where it was drawn, the same pace rule as a model's
// frame (model_lerp.hpp's `frame_pace`). Every rule that snaps an entity ends its stride too.
//
// What is not drawn between: a model's animation frame (one frame drawn), the statics, the stats
// and the HUD, the sounds and effects (they belong to the frame that carried them).
//
// The state drawn (`DemoLerp::drawn`) is the last frame read with its moving parts replaced: the
// reader's own state is never written, since the next frame's deltas are read against it.
//
// ge:summary A demo drawn between its last two frames: players, entities and the view interpolated
// ge:keywords demo interpolation lerp smooth smoothing jerky stutter multiview mvd playdemo
// ge:keywords demovideo between-frames one-frame-behind teleport snap respawn gib setangle
// ge:keywords lerp_frame lerp_at lerp_angle lerp_player lerp_entity demo_lerp shortest-angle
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_lerp_test.cpp
#pragma once

#include <vector>

#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/model_lerp.hpp"

namespace shooters_edge::client {

inline constexpr float kPlayerSnapUnits = 150;  // a player moving farther in a frame: not between
inline constexpr float kEntitySnapUnits = 200;  // an entity moving farther in a frame: not between
inline constexpr double kMostLerpGap = 0.25;    // frames further apart are not drawn between, s

// What one frame left that moves: the time, the view, the players and entities.
struct DemoPose {
  double time = 0;  // the demo's own clock, s
  int levels = 0, player = -1;
  bool intermission = false;
  uint32_t set_angles = 0;
  sh::Vec3f eye, angles;
  std::vector<DemoEntity> entities, players;
};

// An entity's stride (the rule above), by its number.
struct DemoStride {
  double moved = -1;      // the frame time it last moved in, s
  bool stepping = false;  // drawn over its stride's time from `origin`, `angles`
  sh::Vec3f origin, angles;
  double at = 0, span = kFrameSpan;  // the stride's start and length on the demo's clock, s
};

struct DemoLerp {
  bool strides = true;  // draw a still thing's moves over their time (r_lerpmove)
  std::vector<DemoStride> stride;
  DemoPose from, to;     // the frame before the last read, and the last
  bool between = false;  // `from` may be drawn from (false: a level's first frame, a seek)
  DemoState drawn;       // the last `lerp_at`'s
  double f = 1;          // the last `lerp_at`'s fraction (1: `to` as read)
  int snapped = 0;       // the last `lerp_at`'s players and entities drawn at `to` by a rule
};

// The frame just read into `s` kept as `to` (the last `to` becomes `from`). `cut` (a seek) or a
// new level starts over: the next picture shows `to` as read.
void lerp_frame(DemoLerp& l, const DemoState& s, bool cut = false);
// `s` (the last frame read, which `lerp_frame` kept) as it stands at `time` on the demo's clock:
// `l.drawn`, its players, entities, eye and angles between `from` and `to`.
const DemoState& lerp_at(DemoLerp& l, const DemoState& s, double time);

// The fraction a picture at `time` stands between `from` and `to` (1 when they are not to be drawn
// between: a cut, a gap over `kMostLerpGap`, time not moving forward).
double lerp_fraction(const DemoLerp& l, double time);
// `a` to `b` by `f`, degrees, the shortest way round (180 apart: the other way round from `a`).
float lerp_angle(float a, float b, float f);
// One player between two frames by the rules above; `angles` false keeps `b`'s angles (his view
// was set). True when drawn between, false when drawn at `b`.
bool lerp_player(const DemoEntity& a, const DemoEntity& b, float f, bool angles, DemoEntity& out);
// One entity between two frames by the rules above. True when drawn between.
bool lerp_entity(const DemoEntity& a, const DemoEntity& b, float f, DemoEntity& out);

}  // namespace shooters_edge::client
