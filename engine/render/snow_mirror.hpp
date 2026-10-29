// THE SNOW LAYER ON THE GPU (docs/snow.md § Drawn): a mirror of one `snow::Layer` that a
// material or a compute pass reads through render/wgsl/snow_mirror.wgsl, so the picture's snow is
// the physics' snow.
//
// - THE FINE WINDOW: the live fine tiles in a square of `window` tiles a side round the eye, each
//   uploaded into a slot of the ATLAS as it is stored (a tile's 4096 cells, eight bytes each, so
//   the GPU reads the same integers the physics wrote), behind a PAGE TABLE of the window's tiles
//   (atlas slot + 1, 0 for none). A tile is uploaded again only when the layer stamps it changed
//   (`Layer::tile_stamp`).
// - THE COARSE VIEW: one block of 4×4 coarse cells for every tile of the layer that holds any
//   coarse snow: the layer's own coarse blocks, and for a live fine tile what folding it would
//   leave (`snow::summary_of`), so a fine tile outside the window draws worn, not untouched. Behind
//   a table of every tile (block + 1, 0 for none); re-sent only when its source's stamp changes.
// - THE COVER: the layer's cover read at its region's origin (a constant cover; a cover drawn
//   from grids reads the same constant until a material samples its own grids), and the named
//   snows' densities, for the packed share.
//
// The bindings are group 3, 0 to 4 (`layout`): a material that reads the mirror puts its own after
// them. `update_snow_mirror` once a frame, after the step's presses and before the passes that
// read it. Reads only the layer; draws nothing.
//
// ge:summary The snow layer mirrored on the GPU: a fine atlas behind a page table, the coarse view
// ge:keywords snow snow-mirror gpu-mirror atlas page-table fine-window coarse-view upload stamp
// ge:keywords create_snow_mirror update_snow_mirror snow_mirror summary_of storage-buffer
// ge:skill render snow
// ge:test tests/render_snow_mirror_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "snow/layer.hpp"

namespace ge::render {

struct SnowMirrorSpec {
  int window = 16;  // tiles a side of the fine window round the eye (16: 102 m at a 10 cm cell)
};

// What an update sent.
struct SnowMirrorStats {
  uint32_t tiles = 0;   // fine tiles uploaded
  uint32_t blocks = 0;  // coarse blocks uploaded
  uint64_t bytes = 0;   // every byte written, tables included
};

struct SnowMirror {
  int window = 0;
  int tx0 = 0, tz0 = 0;  // the window's first tile
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;
  wgpu::Buffer info, page, atlas, coarse_page, coarse;
  uint32_t coarse_capacity = 0;  // blocks the coarse buffer holds
  // The CPU's record of what the GPU holds.
  std::vector<int32_t> atlas_tile;    // atlas slot: the tile index it holds, -1 free
  std::vector<uint64_t> atlas_stamp;  // the stamp it was sent at
  std::vector<uint32_t> page_cpu;     // window tile: atlas slot + 1
  std::vector<uint32_t> block_of;     // tile index: coarse block + 1
  std::vector<uint64_t> block_stamp;  // tile index: the stamp its block was sent at (0: clear)
  uint32_t blocks = 0;                // coarse blocks handed out
  SnowMirrorStats last, total;
  bool tables_dirty = true;
  // The probe (labs and tests), made on first use.
  wgpu::BindGroupLayout probe_layout;
  wgpu::ComputePipeline probe_raw, probe_columns;
};

// The mirror of `layer`: its buffers made, nothing uploaded yet.
void create_snow_mirror(gpu::Device& device, SnowMirror& mirror, const snow::Layer& layer,
                        const SnowMirrorSpec& spec = {});
// The window recentred on (eye_x, eye_z) and everything the layer changed since uploaded.
void update_snow_mirror(gpu::Device& device, SnowMirror& mirror, const snow::Layer& layer,
                        double eye_x, double eye_z);

// What the GPU reads at sample (c, r), as the CPU works it out from the layer and the mirror: the
// depth and mass (mm and units; summed over `samples` for a coarse cell), the cell's kind, flags
// and minute packed as kind | flags << 8 | minute << 16, and the source (0 the cover, 1 a fine
// cell, 2 a coarse cell). The test and the lab hold the GPU to it to the bit.
struct SnowRaw {
  uint32_t depth = 0, mass = 0, bits = 0, source = 0, samples = 1;
};
SnowRaw snow_mirror_expect(const SnowMirror& mirror, const snow::Layer& layer, int c, int r);

// Labs and tests (blocks; makes its pipelines on first use): what the GPU's `snow_raw` reads at
// every sample of the `w` by `h` rectangle from (c0, r0), row-major.
std::vector<SnowRaw> read_snow_raw(gpu::Device& device, SnowMirror& mirror, int c0, int r0, int w,
                                   int h);
// The GPU's `snow_column` at each plan point of `xz` (pairs x, z).
struct SnowColumnRead {
  float depth, drop, density, packed, touched, pressed;
};
std::vector<SnowColumnRead> read_snow_columns(gpu::Device& device, SnowMirror& mirror,
                                              std::span<const float> xz);

}  // namespace ge::render
