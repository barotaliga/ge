// THE CLOUD IN THE PICTURE: the layers of `FrameView::clouds` (render/view.hpp's `Clouds`)
// marched through and laid over the frame, and the shadow each casts, each place its own share
// of the sun.
//
// - THE SHAPE is one tileable noise, a 64^3 fbm of value noise made here on the CPU (its bytes
//   the same on every machine), its base lattice eight cells across the tile. A layer reads it
//   twice: once flat at the cell's scale, the noise's third axis the layer's age, for its COLUMN
//   (where the noise passes the cut its cover sets a column stands, `cloud_cut` the noise's own
//   quantile, so the cover is a true share of the sky); and once in three dimensions at a
//   quarter of the cell for the DETAIL that eats the column's edge. Within the slab a HEAP's top
//   grows into the cell from a flat base (the fuller the column, the taller); a SHEET thickens
//   about the slab's middle.
// - THE WIND carries each layer along its heading (the drift, kept within the noise's tile in
//   double) and its shape changes as it goes (the age).
// - THE LIGHT on a point of cloud is the key light through the air to its height (the sky's
//   transmittance), through the cloud toward it (four reads at growing steps, Beer's law with a
//   slower lobe for the light scattered on), with a forward and a back lobe, darker at a thin
//   edge seen from the light's side (the powder); the shaded part takes the sky's light, greyer
//   toward the base. Far cloud melts into the sky behind it.
// - THE SHADOW on the ground (`frame.wgsl`'s `cloud_shadow`, which every lit surface's
//   `sun_light` reads): each layer's column where the beam toward the key light crosses its
//   middle, taken off the light as exp(-column * the layer's shadow depth).
// - THE PASS ("clouds", after the sky): a full-screen triangle that marches each pixel's ray up
//   to what the depth target holds, and lays the cloud over the HDR target premultiplied (its
//   light added, what is behind kept by its transmittance). With no layer it runs empty.
//
// ge:summary The cloud's layers marched and laid over the frame, their noise and their shadow
// ge:keywords render clouds cloud-layer heap sheet cumulus cirrus raymarch cloud-shadow
// ge:keywords cloud-noise value-noise fbm cover cut quantile powder beer drift cloud_rows
// ge:keywords cloud_cut cloud_column encode_clouds
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

// The noise's edge in texels, and its base lattice's cells across the tile.
inline constexpr int kCloudNoiseSize = 64;
inline constexpr int kCloudTileCells = 8;

// The noise, x fastest then y then z, 0..255: made once, the same bytes everywhere.
const std::vector<uint8_t>& cloud_noise();
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

struct CloudPass {
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;
  uint32_t p_draw = 0;
};
// The pass's pipeline (into an HDR target), added to the manifest.
void create_clouds(gpu::Device& device, gpu::PipelineSet& pipelines, CloudPass& clouds,
                   const wgpu::BindGroupLayout& frame);
// Its group over the frame's depth (again when the targets are made again).
void bind_cloud_targets(gpu::Device& device, CloudPass& clouds, const FrameTargets& targets);
// The pass ("clouds"): empty when `view` has no layer.
void encode_clouds(const CloudPass& clouds, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                   const wgpu::BindGroup& frame, const FrameView& view);

}  // namespace ge::render
