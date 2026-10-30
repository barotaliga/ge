// THE SNOW'S FINE MARKS (docs/snow.md § Drawn): every groove, rut, footprint and crater a snow
// layer has been pressed with, kept on the GPU in world space at a few centimetres a texel, so a
// ski's 7 cm groove reads as a groove although the simulation's cell is 10 cm. A material reads
// the maps beside the layer's mirror (render/snow_mirror.hpp): the mirror is the snow's mass and
// depth, the marks are the detail on top.
//
// TWO MAPS, because one cannot be both fine enough and big enough. A ski's groove is a few
// centimetres wide, and a map that fine over a whole mountain would be some sixty thousand texels
// a side. So:
// - the COARSE map spans the whole region at about a metre and a half a texel; every stamp goes
//   into it, scaled by how much of a texel it covers, so a track on the far side of the region
//   still reads as a faint line from afar;
// - the FINE map is a window `MarkOptions::fine_span` m square round the camera at a few
//   centimetres a texel, where two grooves read as two grooves.
//
// THE HISTORY. Every stamp is also kept on the CPU (48 bytes each), filed by where it lies. When
// the camera leaves the middle of the fine window, the window moves to centre on it and is
// REDRAWN from the history: cleared, and every stamp ever laid inside it stamped again. So a
// track laid out of the window is there, as sharp as the day it was cut, when the camera comes
// down it: nothing is copied from the coarse map, and nothing is lost to a move.
//
// A STAMP is a capsule, a box or an ellipse on the plan (snow_marks.wgsl says how each is
// measured) drawn as one instanced quad into a map with MAX blending: a groove ridden twice is as
// deep as the deeper of the two, and stamps that overlap in one draw cannot race (the blend is the
// GPU's own read-modify-write). Its cross-section is `depth` (1 - u^wall) across the groove (u: 0
// on the axis, 1 at the outline) and a berm `berm` high just outside it, out to `kMarkBermReach`
// of the half width again. `marks_from_records` makes them from a layer's imprint records, so a
// game that presses snow gets its marks with no stamps of its own; a game that lays finer stamps
// than its presses (a ski's two edges, say) adds those itself.
//
// THE TEXEL (RGBA8, rows along +z):
//   r  sqrt(depth / kMarkDepth)  (the root keeps a centimetre's scuff some 25 levels deep)
//   g  sqrt(berm / kMarkBerm)
//   b  pressed: 1 on a groove's floor (the snow the contact packed), 0 off any
//   a  0 (spare)
// A material reads both maps through `MarkRect` (the two rects, one uniform each).
//
// ge:summary The snow's fine marks: coarse and fine world-space maps of every stamp, max blended
// ge:keywords snow snow-marks fine-marks trail-map tracks grooves berm pressed-snow stamp capsule
// ge:keywords box ellipse max-blend history fine-window coarse-map restamp recentre imprint-records
// ge:keywords create_snow_marks add_mark_stamps prepare_snow_marks encode_snow_marks
// ge:keywords marks_from_records mark_window_at
// ge:skill render snow
// ge:test tests/render_snow_marks_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "render/frame.hpp"
#include "snow/layer.hpp"

namespace ge::render {

// The deepest groove and the highest berm the maps keep, m (their channels' full scale).
inline constexpr double kMarkDepth = 1.0;
inline constexpr double kMarkBerm = 0.15;
// How far past a groove's edge its berm reaches, a share of its half width.
inline constexpr double kMarkBermReach = 0.9;

// A stamp's outline: a capsule from a to b (a disc where they meet), or a box or an ellipse
// centred between them with b - a along its length.
// NOLINTNEXTLINE(performance-enum-size): the GPU reads it as a u32, in the stamp itself.
enum class MarkShape : uint32_t { capsule = 0, box = 1, ellipse = 2 };

// One stamp, as the GPU reads it (snow_marks.wgsl's `Stamp`).
struct MarkStamp {
  float ax, az, bx, bz;
  float half_width;  // m
  float depth;       // m, on the axis
  float berm;        // m
  float wall;        // the cross-section's power: 2 round, 10 square
  MarkShape shape = MarkShape::capsule;
  uint32_t pad[3] = {0, 0, 0};
};
static_assert(sizeof(MarkStamp) == 48, "match snow_marks.wgsl's Stamp");

struct MarkOptions {
  uint32_t fine_size = 2048;         // the fine window's texels a side
  double fine_span = 96;             // the fine window's side, m
  uint32_t coarse_size = 2048;       // the coarse map's texels a side (it spans the region)
  uint32_t capacity = 65536;         // the stamps one draw of the maps can carry
  std::string label = "snow_marks";  // the targets', the pipeline's and the passes' name
};

// Where the fine window lies: its corner (least x, least z) on the plan, m.
struct MarkWindow {
  double x0 = 0, z0 = 0;
};

// The window centred on (x, z), its corner on a grid of an eighth of the span (so a redraw
// lands every stamp on the same texels it had).
MarkWindow mark_window_at(double span, double x, double z);
// Whether a camera at (x, z) has left the window's middle half (time to move it).
bool mark_window_moves(const MarkWindow& w, double span, double x, double z);
// Whether a stamp's quad (its outline, its berm and a texel) reaches into the square
// [x0, x0 + span] x [z0, z0 + span].
bool mark_stamp_meets(const MarkStamp& s, double x0, double z0, double span);

// The stamps a layer's imprint records leave, appended to `out`: each record's outline as it was
// pressed (a disc as a capsule from its centre to itself), its sink as the depth, a berm where it
// threw snow aside, and a wall by how it moved (a slide's round groove, a step's square print).
void marks_from_records(std::span<const snow::ImprintRecord> records, std::vector<MarkStamp>& out);

// A map's rect for the shaders (snow_marks.wgsl's `MarkRect`).
struct MarkRect {
  float x0, z0, span, texel;  // m
};

struct SnowMarks {
  MarkOptions options;
  gpu::Target fine, coarse;
  wgpu::Buffer stamps;    // storage: this frame's fine stamps, then its coarse ones
  wgpu::Buffer rects[2];  // uniform: the fine map's rect, the coarse map's
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup groups[2];  // the stamp pass's, over the fine map's rect and the coarse map's
  uint32_t p_stamp = 0;
  std::string pass_names[2];  // "<label>.fine", "<label>.coarse"
  MarkWindow window;
  MarkRect coarse_rect{};
  bool placed = false;       // the window has been placed since the marks began
  bool fine_clear = true;    // the fine map's next pass clears it (a redraw from the history)
  bool coarse_clear = true;  // the coarse map's next pass clears it
  std::vector<MarkStamp> history;
  std::vector<std::vector<uint32_t>> buckets;  // the history's indices by the cell its centre is in
  int bucket_cols = 0, bucket_rows = 0;
  std::vector<MarkStamp> pending;  // laid since the last draw
  std::vector<MarkStamp> staged;   // the stamps the next encode draws: fine, then coarse
  uint32_t fine_count = 0, coarse_count = 0;
};

// The maps over the square of side `extent` m from (x0, z0), their pipeline (into `pipelines`,
// before the load ends) and their groups. Both maps start clear.
void create_snow_marks(gpu::Device& device, gpu::PipelineSet& pipelines, SnowMarks& marks,
                       double x0, double z0, double extent, const MarkOptions& options = {});

// New stamps, laid since the last frame: drawn into both maps at the next encode and kept.
void add_mark_stamps(SnowMarks& marks, std::span<const MarkStamp> stamps);

// Forget every mark (a new run on the same ground): both maps are cleared at the next encode.
void clear_snow_marks(SnowMarks& marks);

// The frame's decisions and uploads, for a camera at (eye_x, eye_z): moves the window when the
// camera has left its middle (a redraw from the history), and stages this frame's stamps. When
// more stamps wait than `capacity` (a lab that laid a whole run between frames), the overflow is
// drawn here in command buffers of its own.
void prepare_snow_marks(gpu::Device& device, const gpu::PipelineSet& pipelines, SnowMarks& marks,
                        double eye_x, double eye_z);

// The two passes, "<label>.fine" then "<label>.coarse": each opens every frame, drawing what
// prepare staged (none: a pass that loads and stores).
void encode_snow_marks(SnowMarks& marks, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                       wgpu::CommandEncoder& encoder);

// The fine map's rect now.
MarkRect fine_rect(const SnowMarks& marks);

}  // namespace ge::render
