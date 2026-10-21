// THE GROUND'S OWN SHADOW: a horizon map over a heightfield (docs/rendering.md § Lighting and
// shadows). For each texel of a grid over the field, a compute pass marches toward one sun
// bearing and stores the tangent of the highest elevation angle the ground reaches that way (the
// planet's curvature included). A point is lit when the sun stands above its horizon, so the
// sun's ELEVATION can change with no rebake; only a change of BEARING needs a new map.
//
// THE BEARINGS ARE QUANTISED (kHorizonBearingStep) and two maps are kept: the bearings either
// side of the sun's, in the slot of their index's parity, blended by where the sun lies between
// them, so a shadow's edge glides as the sun turns. A step of the sun into the next interval
// needs one new map; `prepare_horizon` decides which, `encode_horizon` bakes it, and a map not
// rebaked keeps its bits. `Horizon::bakes` counts the bakes (the labs' redraw counter).
//
// THE GRID: every `stride` samples of the field, so the march reads the field's own bilinear
// heights (mirrored past the edge, as the terrain draws them). The terrain's draw reads the
// maps through `TerrainParams` and `horizon_light` in render/wgsl/terrain_common.wgsl.
//
// ge:summary The terrain's horizon map: bearing slots, the bake decision, the bake pass, the CPU
// reference
// ge:keywords render horizon-map shadow terrain-shadow ridge-shadow sun-bearing bake blend
// ge:keywords horizon_tan prepare_horizon encode_horizon read_horizon bearing_index
// ge:skill render
// ge:test tests/render_horizon_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "core/heightfield.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/view.hpp"

namespace ge::render {

inline constexpr int kHorizonBearings = 180;  // even, so a bearing's slot is its parity
inline constexpr double kHorizonBearingStep = 2.0 * 3.14159265358979323846 / kHorizonBearings;
inline constexpr int kHorizonSteps = 96;            // the march's samples (horizon.wgsl)
inline constexpr double kEarthRadiusM = 6360000.0;  // the ground's curvature, m
// The shadow's soft edge, in tangent of elevation: the sun's disc (0.27 degrees of radius) and
// the grid's interpolation.
inline constexpr float kHorizonPenumbra = 0.008f;

struct HorizonSettings {
  int stride = 2;           // field samples a texel
  double range_m = 8000.0;  // how far the march looks
};

// The march's geometry, for the GPU bake and the CPU reference alike: the first step and the
// ratio each step grows by, from one grid cell out to the range in kHorizonSteps samples.
struct HorizonMarch {
  double first_m, ratio, range_m;
};
HorizonMarch horizon_march(double cell_m, double range_m);

// The bearing index the sun's azimuth (rad, a world heading) falls in, and the share of the
// way to the next.
int bearing_index(double azimuth, double* fraction);
double bearing_of(int index);

struct Horizon {
  int width = 0, height = 0;  // texels
  double origin_x = 0, origin_z = 0, cell = 1;
  HorizonMarch march{};
  wgpu::Texture maps[2];
  wgpu::TextureView views[2];
  wgpu::Buffer bake_params[2];
  wgpu::BindGroupLayout bake_layout;
  wgpu::BindGroup bake_groups[2];
  uint32_t p_bake = 0;
  int held[2] = {-1, -1};         // the bearing index each slot holds (-1: none yet)
  int due[2] = {-1, -1};          // the bearing index to bake into each slot this frame
  int bakes = 0;                  // maps baked so far
  float blend[4] = {0, 0, 0, 0};  // terrain_common.wgsl's `horizon`: share, low slot, penumbra, on
};

// The maps' textures, the bake's pipeline (added to the manifest) and its groups. The bake reads
// the terrain's params block and heights.
void create_horizon(gpu::Device& device, gpu::PipelineSet& pipelines, Horizon& horizon,
                    const core::Heightfield& field, const HorizonSettings& settings,
                    const wgpu::Buffer& terrain_params, const wgpu::TextureView& heights);
// Which maps this frame needs (the bearings either side of the sun's) and which of them to bake;
// fills `blend`. CPU only: it uploads the due bakes' parameters when `device` is not null.
void prepare_horizon(gpu::Device* device, Horizon& horizon, const FrameView& view);
// The due bakes ("terrain.horizon"); a frame with nothing due opens the pass and dispatches
// nothing.
void encode_horizon(Horizon& horizon, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                    wgpu::CommandEncoder& encoder);

// The CPU reference of one texel's bake: the tangent of the highest elevation the field
// reaches from (x, z) toward `bearing`, marched as the GPU marches (mirrored past the edge).
double horizon_tan(const core::Heightfield& field, double x, double z, double bearing,
                   const HorizonMarch& march);

// Labs and tests: a slot's map read back (row-major, `width` x `height` tangents). Blocks.
std::vector<float> read_horizon(gpu::Device& device, const Horizon& horizon, int slot);

}  // namespace ge::render
