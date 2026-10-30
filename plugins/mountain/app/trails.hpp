// THE TRAIL MAP: every track any skier has laid this run, kept on the GPU in world space and read
// by the snow's material (snow.hpp), which shades the grooves, the berms and the pressed snow.
// The mechanism is the engine's fine marks (render/snow_marks.hpp: the coarse map over the whole
// mountain, the fine window round the camera redrawn from the history, the stamps drawn with MAX
// blending, the texel's encoding); this header names it for the mountain, spans the maps over a
// level's ground and keeps the passes' names "trails.fine" and "trails.coarse". The stamps are
// skier/app/tracks.hpp's: a ski's groove, finer than the presses the skier's physics makes.
//
// ge:summary The trail map: the engine's fine marks spanned over a level, named for the mountain
// ge:keywords trails trail-map tracks grooves berm pressed-snow stamp create_trails
// ge:keywords add_trail_stamps prepare_trails encode_trails trail_window snow-marks
// ge:skill mountain render
// ge:test tests/mountain_trails_test.cpp
#pragma once

#include <algorithm>
#include <span>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/frame.hpp"
#include "render/snow_marks.hpp"

namespace ge::mountain::app {

inline constexpr double kTrailDepth = render::kMarkDepth;
inline constexpr double kTrailBerm = render::kMarkBerm;
inline constexpr double kTrailBermReach = render::kMarkBermReach;

using TrailStamp = render::MarkStamp;
using TrailWindow = render::MarkWindow;
using TrailRect = render::MarkRect;
using Trails = render::SnowMarks;

struct TrailOptions {
  uint32_t fine_size = 2048;    // the fine window's texels a side
  double fine_span = 96;        // the fine window's side, m
  uint32_t coarse_size = 2048;  // the coarse map's texels a side (it spans the map)
  uint32_t capacity = 65536;    // the stamps one draw of the maps can carry
};

inline TrailWindow trail_window_at(double span, double x, double z) {
  return render::mark_window_at(span, x, z);
}
inline bool trail_window_moves(const TrailWindow& w, double span, double x, double z) {
  return render::mark_window_moves(w, span, x, z);
}
inline bool trail_stamp_meets(const TrailStamp& s, double x0, double z0, double span) {
  return render::mark_stamp_meets(s, x0, z0, span);
}

// The maps over `level`'s ground (the coarse map spans its longer side), named "trails".
inline void create_trails(gpu::Device& device, gpu::PipelineSet& pipelines, Trails& trails,
                          const mapgen::Level& level, const TrailOptions& options = {}) {
  const auto& g = level.ground;
  const double extent = std::max(g.cols - 1, g.rows - 1) * g.cell;
  render::MarkOptions o;
  o.fine_size = options.fine_size;
  o.fine_span = options.fine_span;
  o.coarse_size = options.coarse_size;
  o.capacity = options.capacity;
  o.label = "trails";
  render::create_snow_marks(device, pipelines, trails, g.origin_x, g.origin_z, extent, o);
}
inline void add_trail_stamps(Trails& trails, std::span<const TrailStamp> stamps) {
  render::add_mark_stamps(trails, stamps);
}
inline void clear_trails(Trails& trails) { render::clear_snow_marks(trails); }
inline void prepare_trails(gpu::Device& device, const gpu::PipelineSet& pipelines, Trails& trails,
                           double eye_x, double eye_z) {
  render::prepare_snow_marks(device, pipelines, trails, eye_x, eye_z);
}
inline void encode_trails(Trails& trails, const gpu::PipelineSet& pipelines,
                          render::FrameRecorder& rec, wgpu::CommandEncoder& encoder) {
  render::encode_snow_marks(trails, pipelines, rec, encoder);
}
inline TrailRect fine_rect(const Trails& trails) { return render::fine_rect(trails); }

}  // namespace ge::mountain::app
