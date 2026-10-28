// THE CLOUD IN THE PICTURE: the layers of `FrameView::clouds` (render/view.hpp's `Clouds`)
// marched through and laid over the frame, and the shadow each casts, each place its own share
// of the sun.
//
// - THE SHAPE is one tileable noise, a 64^3 texture made here on the CPU (its bytes the same on
//   every machine): in red an fbm of value noise, its base lattice eight cells across the tile;
//   in green BILLOWS, cellular noise (1 at each cell's point, falling to the seams between) at
//   4, 8 and 16 cells. A layer reads red flat at the cell's scale, the third axis its age, for
//   its COLUMN: where the noise passes the cut its cover sets a column stands (`cloud_cut` the
//   noise's own quantile, so the cover is a true share of the sky), full within kColumnRamp past
//   it; with PATCHES a read at four times the cell must pass its middle too, so the cells gather
//   into islands (a mackerel sky). Within the slab a HEAP's top grows into the cell from a flat
//   base, a dome over the column's core; a SHEET thickens about the slab's middle, a lens where
//   the column is fullest; a storm's ANVIL spreads near the top where the same noise passes a
//   lower cut. Two reads in three dimensions at a tenth of the cell are the DETAIL that eats the
//   edge, as deep as the layer's bite: at the billows' seams for a heap or a sheet (their bulges
//   kept: the cauliflower), in the value noise combed along the wind for ice (its hairs). A
//   heap's edge is crisp past the bite, a sheet's soft.
// - THE WIND carries each layer along its heading (the drift, kept within the noise's tile in
//   double) and its shape changes as it goes (the age).
// - THE LIGHT on a point of cloud is the key light through the air to its height (the sky's
//   transmittance), through the cloud toward it (four reads at growing steps: Beer's law for the
//   light that comes straight through, and for the many orders scattered on, a diffusion's one
//   over the depth, so a deck's base is grey and not black), with a forward and a back lobe and,
//   in ice, the 22 degree halo, darker at a thin edge seen from the light's side (the powder);
//   the shaded part takes the sky's light, greyer toward the base and dimmed by the cloud over
//   it. A long path through a sheet (seen far off, or from under it) takes up to three times
//   the steps, bunched where the ray enters. Far cloud melts into the sky behind it.
// - THE SHADOW on the ground (`frame.wgsl`'s `cloud_shadow`, which every lit surface's
//   `sun_light` reads): each layer's column where the beam toward the key light crosses its
//   middle, taken off the light as exp(-column * the layer's shadow depth).
// - THE PASSES ("clouds.march" and "clouds", after the sky): at a QUARTER of the pixels, a
//   full-screen triangle over a half-size target marches each two by two's ray to the farthest of
//   its depths (a ridge's edge keeps the cloud behind it), keeping the distance it marched to;
//   then each pixel of the frame takes the four half-size pixels round it, weighted by the
//   bilinear footprint and by how near each one's distance is to its own, and a pixel none of
//   them matches (the near side of an edge) is marched itself. SPREAD over frames
//   (`Clouds::spread`, with the last frame's camera), one of each two by two of the half-size
//   pixels marches a frame in turn ("clouds.fresh", over a target a quarter of the half-size
//   one, so the pixels that march are neighbours and a GPU's lanes march together) and the other
//   three reproject the last frame's half-size picture where their cloud stood, marching after
//   all where that history is off the screen, stale or over another scene: the two half-size
//   sets swap each frame. At EVERY pixel (`Clouds::quarter`
//   false), "clouds" marches each pixel and "clouds.march" runs empty. Either way the cloud is
//   laid over the HDR target premultiplied (its light added, what is behind kept by its
//   transmittance); with no layer both run empty.
//
// ge:summary The cloud's layers marched and laid over the frame, their noise and their shadow
// ge:keywords render clouds cloud-layer heap sheet cumulus cirrus raymarch cloud-shadow
// ge:keywords billows worley cellular-noise cauliflower anvil patches halo diffusion corona
// ge:keywords aureole diffraction airy bessel moon-glow
// ge:keywords cloud-noise value-noise fbm cover cut quantile powder beer drift cloud_rows
// ge:keywords cloud_cut cloud_column encode_clouds quarter-resolution half-resolution upsample
// ge:keywords depth-aware-upsample bilateral temporal reprojection spread-over-frames history
// ge:skill render
// ge:test tests/render_clouds_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/view.hpp"

namespace ge::render {

// How far past the cut the noise climbs to a full column (0..1 of the noise).
inline constexpr double kColumnRamp = 0.3;
// The noise's edge in texels, and its base lattice's cells across the tile.
inline constexpr int kCloudNoiseSize = 64;
inline constexpr int kCloudTileCells = 8;

// The noise, x fastest then y then z, 0..255: made once, the same bytes everywhere.
const std::vector<uint8_t>& cloud_noise();
// The billows beside it (the texture's second channel): cellular noise, 1 at each cell's point
// falling to the boundaries between, at 4, 8 and 16 cells across the tile.
const std::vector<uint8_t>& cloud_detail();
// The noise's value (0..1) at a point of its tile (0..1 a side), trilinear and wrapped, as the
// GPU samples it.
double cloud_noise_at(double x, double y, double z);
// The noise's cut a layer of `cover` stands where it passes (its quantile).
double cloud_cut(double cover);
// How full a layer's column is (0..1) over the world point (x, z): the shader's `column`.
double cloud_column(const CloudLayer& layer, double x, double z);

// The frame block's rows for `clouds` (FrameUniforms::cloud_info and clouds).
void cloud_rows(const Clouds& clouds, float info[4], float rows[][4]);

// The noise as a texture with its wrapping sampler (bound in the frame's group 0).
struct CloudNoise {
  wgpu::Texture texture;
  wgpu::TextureView view;
  wgpu::Sampler sampler;
};
CloudNoise create_cloud_noise(gpu::Device& device);

// How many frames the spread's stamp counts before it wraps (FrameUniforms::cloud_info[3]).
inline constexpr uint64_t kCloudStamps = 4096;

struct CloudPass {
  wgpu::BindGroupLayout layout, up_layout, march_layout;
  // The march's groups (the depth, the other set, its history, and the fresh targets) and the
  // upsample's (the depth and its own set), by the set a frame writes: its number's parity.
  wgpu::BindGroup group, march_group[2], up_group[2];
  // The spread's fresh march, a quarter of the half-size target: the cloud (RGBA16F), and the
  // distance it marched to and its cloud lies at (RG32F).
  wgpu::Texture fresh, fresh_dist;
  wgpu::TextureView fresh_view, fresh_dist_view;
  // The half-size targets, two sets that swap each frame: the cloud (RGBA16F), and (RGBA32F) the
  // distance each pixel marched to, the distance its cloud lies at and the stamp of its frame.
  wgpu::Texture quarter[2], quarter_dist[2];
  wgpu::TextureView quarter_view[2], quarter_dist_view[2];
  uint32_t quarter_width = 0, quarter_height = 0;
  uint32_t p_draw = 0, p_fresh = 0, p_march = 0, p_upsample = 0;
};
// The pass's pipeline (into an HDR target), added to the manifest.
void create_clouds(gpu::Device& device, gpu::PipelineSet& pipelines, CloudPass& clouds,
                   const wgpu::BindGroupLayout& frame);
// Its half-size targets and its groups over the frame's depth (again when the targets are made
// again).
void bind_cloud_targets(gpu::Device& device, CloudPass& clouds, const FrameTargets& targets);
// The passes ("clouds.fresh", "clouds.march", then "clouds"): empty when `view` has no layer
// ("clouds.fresh" unless spread, "clouds.march" unless at a quarter); `block` is the frame block
// `frame` holds, whose cloud_info says whether the march is spread this frame.
void encode_clouds(const CloudPass& clouds, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                   const wgpu::BindGroup& frame, const FrameView& view, const FrameUniforms& block);

}  // namespace ge::render
