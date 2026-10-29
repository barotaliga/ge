// THE WET LENS (docs/water.md § Drawn, M24h2): what the water leaves on a camera or an eye
// that comes up out of it, and what a splash throws on one, drawn over the finished frame.
//
// - THE STATE is the CPU's and deterministic (`WetLensState`, stepped by `step_wet_lens` off its
//   own `core::Rng`), so a lab digests it and a replay shows the same drops:
//   - THE SHEET: leaving the water, the lens is covered; the sheet's top edge runs down the
//     screen over `drain` s, accelerating as a film does, the view under it rippled.
//   - THE DROPS the sheet leaves as its edge passes: radii between `small` and `big` (screen
//     heights), more small than big. A drop over `stick` slides down, the faster the bigger,
//     wandering a little sideways, and leaves a trail of beads behind it, losing what it leaves;
//     every drop dries over its `life` (a bead quicker), shrinking.
//   - A SPLASH in the air near the eye spatters the lens (`spatter_wet_lens`); going under clears
//     everything.
// - THE PASS ("water.lens", `encode_wet_lens`) reads the frame at the output's size (TAA-U's
//   resolve, or the HDR target) and writes `target` with the sheet and the drops on it: each drop
//   a lens of water showing the scene behind it upside down and blurred (it is out of focus that
//   near the glass), a dark rim and a highlight. Post then reads `wet_lens_view` (the target
//   while anything is on the lens, else the frame itself), so a dry lens costs nothing. It runs
//   after TAA-U, because the drops are fixed to the screen and must not be reprojected.
//
// ge:summary The wet lens: the sheet draining and drops sliding after surfacing
// ge:summary drawn over the frame
// ge:keywords render water lens wet-lens drops droplets sheet film drain surfacing splash spatter
// ge:keywords camera-lens step_wet_lens spatter_wet_lens create_wet_lens encode_wet_lens
// ge:skill render water
// ge:test tests/render_wet_lens_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"

namespace ge::render {

inline constexpr int kLensDrops = 96;  // drops drawn (the biggest first when there are more)

struct WetLensLook {
  double drain = 0.5;    // s the sheet takes to run off the screen's height
  int drops = 40;        // drops the sheet leaves
  double big = 0.045;    // the biggest drop's radius, screen heights
  double small = 0.006;  // the smallest's
  double stick = 0.02;   // drops smaller than this cling
  double slide = 0.5;    // screen heights a second a drop twice `stick` slides at
  double life = 5;       // s a clinging drop of `big` takes to dry (a smaller one less)
  double bead = 0.03;    // screen heights a sliding drop runs between the beads it leaves
  double refract = 1.2;  // how far a drop's view is turned over (1: as wide as the drop)
};

struct LensDrop {
  double x = 0, y = 0;  // the centre: x 0..1 across, y 0..1 down
  double r = 0;         // radius, screen heights
  double born = 0;      // s since the lens left the water when the sheet's edge passed it
  double life = 0;      // s it takes to dry
  double age = 0;       // s since born
  double speed = 0;     // screen heights a second, down
  double ran = 0;       // screen heights since its last bead
};

struct WetLensState {
  WetLensLook look;
  std::vector<LensDrop> drops;
  double since = 1e9;  // s since the lens left the water
  bool under = false;  // the lens is in the water
  core::Rng rng = core::make_rng(0x1e75);
};

// Where the sheet's top edge is, 0 (top) .. 1 (gone), `since` s after leaving the water.
double sheet_edge(const WetLensLook& look, double since);
// One step: `under` says whether the lens is in the water now.
void step_wet_lens(WetLensState& lens, bool under, double dt);
// A splash's drops thrown on the lens: `count` of them, up to `size` screen heights.
void spatter_wet_lens(WetLensState& lens, int count, double size);
// Whether anything is on the lens (the sheet or a drop).
bool wet(const WetLensState& lens);

struct WetLensGpu {
  float info[4];               // the sheet's edge (0..1, 1: none), the time (s), drops, 0
  float drops[kLensDrops][4];  // x, y, radius, the stretch of a sliding drop
};

struct WetLens {
  WetLensState state;
  wgpu::BindGroupLayout layout;
  uint32_t p_draw = 0;
  wgpu::Buffer uniform;
  wgpu::Texture target;
  wgpu::TextureView target_view, input;
  wgpu::BindGroup group;
  WetLensGpu staged{};
  bool active = false;  // this frame's: something is on the lens
};

void create_wet_lens(gpu::Device& device, gpu::PipelineSet& pipelines, WetLens& lens,
                     const wgpu::BindGroupLayout& frame);
// The target at the output's size over the frame it reads (again when the targets change).
void create_wet_lens_target(gpu::Device& device, WetLens& lens, const wgpu::TextureView& input,
                            uint32_t width, uint32_t height);
// The state's drops uploaded, at `time` s.
void prepare_wet_lens(gpu::Device& device, WetLens& lens, double time);
// "water.lens": an empty row when the lens is dry.
void encode_wet_lens(const WetLens& lens, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                     wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame);
// What post reads this frame: the target while the lens is wet, else the frame itself.
const wgpu::TextureView& wet_lens_view(const WetLens& lens);

}  // namespace ge::render
