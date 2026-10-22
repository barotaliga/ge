// THE TRAIL MAP: every track any skier has laid this run, kept on the GPU in world space and read
// by the snow's material (snow.hpp), which shades the grooves, the berms and the pressed snow.
//
// TWO MAPS, because one cannot be both fine enough and big enough. A ski's groove is a few
// centimetres wide, and a map that fine over a whole mountain would be some sixty thousand texels
// a side. So:
// - the COARSE map spans the whole map at about a metre and a half a texel; every stamp of every
//   skier goes into it, scaled by how much of a texel it covers, so a track on the far side of
//   the mountain still reads as a faint line from afar;
// - the FINE map is a window `TrailOptions::fine_span` m square round the camera at a few
//   centimetres a texel, where two grooves read as two grooves.
//
// THE HISTORY. Every stamp is also kept on the CPU (a few dozen bytes each), filed by where it
// lies. When the camera leaves the middle of the fine window, the window moves to centre on it
// and is REDRAWN from the history: cleared, and every stamp ever laid inside it stamped again.
// So a rival's track laid out of the window is there, as sharp as the day it was cut, when the
// player comes down it: nothing is copied from the coarse map, and nothing is lost to a move.
//
// A STAMP is a capsule (skier/app/tracks.hpp says what each one is) drawn as one instanced quad
// into a map with MAX blending: a groove ridden twice is as deep as the deeper of the two, and
// stamps that overlap in one draw cannot race (the blend is the GPU's own read-modify-write). Its
// cross-section is `depth` (1 - u^wall) across the groove (u: 0 on the axis, 1 at the edge) and a
// berm `berm` high just outside it, out to `kTrailBermReach` of the half width again.
//
// THE TEXEL (RGBA8, rows along +z):
//   r  sqrt(depth / kTrailDepth)  (the root keeps a centimetre's scuff some 25 levels deep)
//   g  sqrt(berm / kTrailBerm)
//   b  pressed: 1 on a groove's floor (the snow a ski packed), 0 off any
//   a  0 (spare)
// The material reads both maps through `TrailMaps` (the two rects, one uniform each).
//
// ge:summary The trail map: coarse and fine world-space maps of every track, stamped with max blend
// ge:keywords trails trail-map tracks grooves berm pressed-snow stamp capsule max-blend history
// ge:keywords fine-window coarse-map restamp recentre create_trails add_trail_stamps
// ge:keywords prepare_trails encode_trails trail_window
// ge:skill mountain render
// ge:test tests/mountain_trails_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/frame.hpp"

namespace ge::mountain::app {

// The deepest groove and the highest berm the map keeps, m (its channels' full scale).
inline constexpr double kTrailDepth = 1.0;
inline constexpr double kTrailBerm = 0.15;
// How far past a groove's edge its berm reaches, a share of its half width.
inline constexpr double kTrailBermReach = 0.9;

// One stamp, as the GPU reads it (trails.wgsl's `Stamp`): a capsule on the plan.
struct TrailStamp {
  float ax, az, bx, bz;
  float half_width;  // m
  float depth;       // m, on the axis
  float berm;        // m
  float wall;        // the cross-section's power: 2 round, 10 square
};
static_assert(sizeof(TrailStamp) == 32, "match trails.wgsl's Stamp");

struct TrailOptions {
  uint32_t fine_size = 2048;    // the fine window's texels a side
  double fine_span = 96;        // the fine window's side, m
  uint32_t coarse_size = 2048;  // the coarse map's texels a side (it spans the map)
  uint32_t capacity = 65536;    // the stamps one draw of the maps can carry
};

// Where the fine window lies: its corner (least x, least z) on the plan, m.
struct TrailWindow {
  double x0 = 0, z0 = 0;
};

// The window centred on (x, z), its corner on a grid of an eighth of the span (so a redraw
// lands every stamp on the same texels it had).
TrailWindow trail_window_at(double span, double x, double z);
// Whether a camera at (x, z) has left the window's middle half (time to move it).
bool trail_window_moves(const TrailWindow& w, double span, double x, double z);
// Whether a stamp's quad (the capsule, its berm and a texel) reaches into the square
// [x0, x0 + span] x [z0, z0 + span].
bool trail_stamp_meets(const TrailStamp& s, double x0, double z0, double span);

// A map's rect for the shaders (trails.wgsl's and snow.wgsl's `TrailRect`).
struct TrailRect {
  float x0, z0, span, texel;  // m
};

struct Trails {
  TrailOptions options;
  gpu::Target fine, coarse;
  wgpu::Buffer stamps;    // storage: this frame's fine stamps, then its coarse ones
  wgpu::Buffer rects[2];  // uniform: the fine map's rect, the coarse map's
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup groups[2];  // the stamp pass's, over the fine map's rect and the coarse map's
  uint32_t p_stamp = 0;
  TrailWindow window;
  TrailRect coarse_rect{};
  bool placed = false;       // the window has been placed since the tracks began
  bool fine_clear = true;    // the fine map's next pass clears it (a redraw from the history)
  bool coarse_clear = true;  // the coarse map's next pass clears it
  std::vector<TrailStamp> history;
  std::vector<std::vector<uint32_t>> buckets;  // the history's indices by the cell its centre is in
  int bucket_cols = 0, bucket_rows = 0;
  std::vector<TrailStamp> pending;  // laid since the last draw
  std::vector<TrailStamp> staged;   // the stamps the next encode draws: fine, then coarse
  uint32_t fine_count = 0, coarse_count = 0;
};

// The maps for `level`'s plan, their pipeline (into `pipelines`, before the load ends) and their
// groups. Both maps start clear.
void create_trails(gpu::Device& device, gpu::PipelineSet& pipelines, Trails& trails,
                   const mapgen::Level& level, const TrailOptions& options = {});

// New stamps, laid since the last frame: drawn into both maps at the next encode and kept.
void add_trail_stamps(Trails& trails, std::span<const TrailStamp> stamps);

// Forget every track (a new run on the same map): both maps are cleared at the next encode.
void clear_trails(Trails& trails);

// The frame's decisions and uploads, for a camera at (eye_x, eye_z): moves the window when the
// camera has left its middle (a redraw from the history), and stages this frame's stamps. When
// more stamps wait than `capacity` (a lab that laid a whole run between frames), the overflow is
// drawn here in command buffers of its own.
void prepare_trails(gpu::Device& device, const gpu::PipelineSet& pipelines, Trails& trails,
                    double eye_x, double eye_z);

// The two passes, "trails.fine" then "trails.coarse": each opens every frame, drawing what
// prepare staged (none: a pass that loads and stores).
void encode_trails(Trails& trails, const gpu::PipelineSet& pipelines, render::FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder);

// The fine map's rect now, and the coarse map's.
TrailRect fine_rect(const Trails& trails);

}  // namespace ge::mountain::app
