// The snow mirror's definitions: the buffers, the window's atlas, the coarse view, the CPU twin.
//
// ge:summary Snow mirror definitions: buffers, the atlas and page table, the coarse view, the twin
// ge:keywords snow snow-mirror atlas page-table coarse-view upload stamp
// ge:skill render snow
// ge:test tests/render_snow_mirror_test.cpp
#include "render/snow_mirror.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/snow_probe.wgsl.hpp"
#include "snow/kinds.hpp"
#include "snow/lod.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;
namespace sn = snow;

constexpr uint64_t kTileBytes = sizeof(sn::Tile::cells);          // 4096 cells, 8 bytes each
constexpr uint64_t kBlockBytes = sizeof(sn::CoarseBlock::cells);  // 16 cells, 12 bytes each
static_assert(sizeof(sn::Cell) == 8 && kTileBytes == 32768);
static_assert(sizeof(sn::CoarseCell) == 12 && kBlockBytes == 192);
static_assert(static_cast<int>(sn::Kind::count) <= 8);

// snow_mirror.wgsl's SnowMirrorInfo.
struct Info {
  float region[4];    // x0, z0, cell, 0
  int32_t dims[4];    // cols, rows, tile_cols, tile_rows
  int32_t window[4];  // tx0, tz0, side, 0
  float cover[4];     // depth m, density kg/m3, packed share, kind
  float kinds[8][4];  // each named snow's loose, packed and cap densities
};

int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

void write_info(gpu::Device& device, const SnowMirror& m, const sn::Layer& layer) {
  Info info{};
  info.region[0] = static_cast<float>(layer.spec.region.x0);
  info.region[1] = static_cast<float>(layer.spec.region.z0);
  info.region[2] = static_cast<float>(layer.spec.cell);
  info.dims[0] = layer.cols;
  info.dims[1] = layer.rows;
  info.dims[2] = layer.tile_cols;
  info.dims[3] = layer.tile_rows;
  info.window[0] = m.tx0;
  info.window[1] = m.tz0;
  info.window[2] = m.window;
  const sn::CoverSample cover =
      sn::cover_at(layer.spec.cover, layer.spec.region.x0, layer.spec.region.z0);
  info.cover[0] = static_cast<float>(cover.depth);
  info.cover[1] = static_cast<float>(cover.density);
  info.cover[2] = static_cast<float>(cover.packed);
  info.cover[3] = static_cast<float>(cover.kind);
  for (int k = 0; k < static_cast<int>(sn::Kind::count); k++) {
    const sn::KindRow& row = sn::kind_row(static_cast<sn::Kind>(k));
    info.kinds[k][0] = static_cast<float>(row.loose);
    info.kinds[k][1] = static_cast<float>(row.packed);
    info.kinds[k][2] = static_cast<float>(row.cap);
  }
  device.queue.WriteBuffer(m.info, 0, &info, sizeof info);
}

void make_group_for(gpu::Device& device, SnowMirror& m) {
  m.group = make_group(device, "snow.mirror", m.layout,
                       {m.info, m.page, m.atlas, m.coarse_page, m.coarse});
}

// The coarse buffer grown to hold `need` blocks, what it held copied across.
void grow_coarse(gpu::Device& device, SnowMirror& m, uint32_t need) {
  if (need <= m.coarse_capacity) return;
  uint32_t cap = std::max(m.coarse_capacity * 2, 64u);
  while (cap < need) cap *= 2;
  wgpu::Buffer bigger =
      make_buffer(device, "snow.mirror.coarse", cap * kBlockBytes, BU::Storage | BU::CopySrc);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(m.coarse, 0, bigger, 0, m.coarse_capacity * kBlockBytes);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  m.coarse = bigger;
  m.coarse_capacity = cap;
  make_group_for(device, m);
}

// The tile at index `at`'s coarse source as a stamp of its state: its live fine tile's, its
// coarse block's, or 0 for nothing.
uint64_t coarse_stamp_of(const sn::Layer& layer, size_t at) {
  const int32_t slot = layer.slots[at];
  if (slot >= 0) {
    return slot < static_cast<int32_t>(layer.tile_stamp.size()) ? layer.tile_stamp[slot] : 1;
  }
  const int32_t block = layer.coarse_slots[at];
  if (block >= 0) {
    return block < static_cast<int32_t>(layer.coarse_stamp.size()) ? layer.coarse_stamp[block] : 1;
  }
  return 0;
}

// That source's block: what folding the live fine tile would leave, the coarse block itself, or
// a clear block.
sn::CoarseBlock coarse_block_of(const sn::Layer& layer, size_t at) {
  if (const int32_t slot = layer.slots[at]; slot >= 0) {
    return sn::summary_of(layer, layer.tiles[slot]);
  }
  if (const int32_t block = layer.coarse_slots[at]; block >= 0) return layer.coarse[block];
  return {};
}

}  // namespace

void create_snow_mirror(gpu::Device& device, SnowMirror& m, const sn::Layer& layer,
                        const SnowMirrorSpec& spec) {
  m.window = std::max(spec.window, 1);
  const auto slots = static_cast<size_t>(m.window) * m.window;
  m.layout = make_layout(device, "snow.mirror",
                         {Slot::uniform, Slot::storage_read, Slot::storage_read, Slot::storage_read,
                          Slot::storage_read},
                         wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Compute);
  m.info = make_buffer(device, "snow.mirror.info", sizeof(Info), BU::Uniform);
  m.page = make_buffer(device, "snow.mirror.page", slots * 4, BU::Storage);
  m.atlas = make_buffer(device, "snow.mirror.atlas", slots * kTileBytes, BU::Storage);
  const auto tiles = static_cast<size_t>(layer.tile_cols) * layer.tile_rows;
  m.coarse_page =
      make_buffer(device, "snow.mirror.coarse_page", std::max<size_t>(tiles, 1) * 4, BU::Storage);
  m.coarse_capacity = 64;
  m.coarse = make_buffer(device, "snow.mirror.coarse", m.coarse_capacity * kBlockBytes,
                         BU::Storage | BU::CopySrc);
  m.atlas_tile.assign(slots, -1);
  m.atlas_stamp.assign(slots, 0);
  m.page_cpu.assign(slots, 0);
  m.block_of.assign(tiles, 0);
  m.block_stamp.assign(tiles, 0);
  m.blocks = 0;
  m.tx0 = m.tz0 = 0;
  m.tables_dirty = true;
  make_group_for(device, m);
  write_info(device, m, layer);
}

void update_snow_mirror(gpu::Device& device, SnowMirror& m, const sn::Layer& layer, double eye_x,
                        double eye_z) {
  SnowMirrorStats st;
  const double tile_m = sn::kTileCells * layer.spec.cell;
  const int etx = static_cast<int>(std::floor((eye_x - layer.spec.region.x0) / tile_m));
  const int etz = static_cast<int>(std::floor((eye_z - layer.spec.region.z0) / tile_m));
  const int tx0 = etx - floor_div(m.window, 2), tz0 = etz - floor_div(m.window, 2);
  const bool moved = tx0 != m.tx0 || tz0 != m.tz0;
  m.tx0 = tx0;
  m.tz0 = tz0;

  // THE FINE WINDOW. The live tile each window place wants, by tile index (-1 none).
  const int w = m.window;
  std::vector<int32_t> want(static_cast<size_t>(w) * w, -1);
  for (int j = 0; j < w; j++) {
    for (int i = 0; i < w; i++) {
      const int tx = tx0 + i, tz = tz0 + j;
      if (tx < 0 || tz < 0 || tx >= layer.tile_cols || tz >= layer.tile_rows) continue;
      const size_t at = static_cast<size_t>(tz) * layer.tile_cols + tx;
      if (layer.slots[at] >= 0) want[static_cast<size_t>(j) * w + i] = static_cast<int32_t>(at);
    }
  }
  // Atlas slots holding a tile no longer wanted are freed.
  for (size_t s = 0; s < m.atlas_tile.size(); s++) {
    if (m.atlas_tile[s] < 0) continue;
    if (std::find(want.begin(), want.end(), m.atlas_tile[s]) == want.end()) m.atlas_tile[s] = -1;
  }
  std::vector<uint32_t> page(want.size(), 0);
  for (size_t k = 0; k < want.size(); k++) {
    const int32_t at = want[k];
    if (at < 0) continue;
    auto it = std::find(m.atlas_tile.begin(), m.atlas_tile.end(), at);
    if (it == m.atlas_tile.end()) {
      it = std::find(m.atlas_tile.begin(), m.atlas_tile.end(), -1);  // the window fits them all
      *it = at;
      m.atlas_stamp[it - m.atlas_tile.begin()] = 0;
    }
    const auto s = static_cast<size_t>(it - m.atlas_tile.begin());
    page[k] = static_cast<uint32_t>(s) + 1;
    const int32_t slot = layer.slots[static_cast<size_t>(at)];
    const uint64_t stamp =
        slot < static_cast<int32_t>(layer.tile_stamp.size()) ? layer.tile_stamp[slot] : 1;
    if (m.atlas_stamp[s] != stamp) {
      device.queue.WriteBuffer(m.atlas, s * kTileBytes, layer.tiles[slot].cells.data(), kTileBytes);
      m.atlas_stamp[s] = stamp;
      st.tiles++;
      st.bytes += kTileBytes;
    }
  }
  if (page != m.page_cpu || m.tables_dirty) {
    m.page_cpu = page;
    device.queue.WriteBuffer(m.page, 0, page.data(), page.size() * 4);
    st.bytes += page.size() * 4;
  }

  // THE COARSE VIEW.
  bool table = m.tables_dirty;
  for (size_t at = 0; at < m.block_of.size(); at++) {
    const uint64_t stamp = coarse_stamp_of(layer, at);
    if (stamp == m.block_stamp[at]) continue;
    // Nothing coarse here any more: the block is cleared, and kept for the tile's next use.
    const sn::CoarseBlock block = coarse_block_of(layer, at);
    if (m.block_of[at] == 0) {
      if (stamp == 0) {
        m.block_stamp[at] = 0;
        continue;
      }
      grow_coarse(device, m, m.blocks + 1);
      m.block_of[at] = ++m.blocks;
      table = true;
    }
    device.queue.WriteBuffer(m.coarse, (m.block_of[at] - 1) * kBlockBytes, block.cells.data(),
                             kBlockBytes);
    m.block_stamp[at] = stamp;
    st.blocks++;
    st.bytes += kBlockBytes;
  }
  if (table) {
    device.queue.WriteBuffer(m.coarse_page, 0, m.block_of.data(), m.block_of.size() * 4);
    st.bytes += m.block_of.size() * 4;
  }
  if (moved || m.tables_dirty) {
    write_info(device, m, layer);
    st.bytes += sizeof(Info);
  }
  m.tables_dirty = false;
  m.last = st;
  m.total.tiles += st.tiles;
  m.total.blocks += st.blocks;
  m.total.bytes += st.bytes;
}

SnowRaw snow_mirror_expect(const SnowMirror& m, const sn::Layer& layer, int c, int r) {
  SnowRaw out;
  if (c < 0 || r < 0 || c >= layer.cols || r >= layer.rows) return out;
  const int tx = c / sn::kTileCells, tz = r / sn::kTileCells;
  const int wi = tx - m.tx0, wj = tz - m.tz0;
  const size_t at = static_cast<size_t>(tz) * layer.tile_cols + tx;
  if (wi >= 0 && wj >= 0 && wi < m.window && wj < m.window && layer.slots[at] >= 0) {
    const sn::Cell& cell = layer.tiles[layer.slots[at]]
                               .cells[(r % sn::kTileCells) * sn::kTileCells + c % sn::kTileCells];
    if (!(cell.flags & sn::kTouched)) return out;
    out.depth = cell.depth_mm;
    out.mass = cell.mass;
    out.bits =
        static_cast<uint32_t>(cell.kind) | uint32_t{cell.flags} << 8 | uint32_t{cell.minute} << 16;
    out.source = 1;
    return out;
  }
  if (coarse_stamp_of(layer, at) == 0) return out;
  const sn::CoarseBlock block = coarse_block_of(layer, at);
  const int i = (c % sn::kTileCells) / sn::kCoarseCells,
            j = (r % sn::kTileCells) / sn::kCoarseCells;
  const sn::CoarseCell& cc = block.cells[j * sn::kCoarsePerTile + i];
  if (!(cc.flags & sn::kTouched)) return out;
  const int c0 = tx * sn::kTileCells + i * sn::kCoarseCells;
  const int r0 = tz * sn::kTileCells + j * sn::kCoarseCells;
  const int pw = std::min(sn::kCoarseCells, layer.cols - c0);
  const int ph = std::min(sn::kCoarseCells, layer.rows - r0);
  out.depth = cc.depth_mm;
  out.mass = cc.mass;
  out.bits = static_cast<uint32_t>(cc.kind) | uint32_t{cc.flags} << 8 | uint32_t{cc.minute} << 16;
  out.source = 2;
  out.samples = static_cast<uint32_t>(pw * ph);
  return out;
}

namespace {

void make_probe(gpu::Device& device, SnowMirror& m) {
  if (m.probe_raw) return;
  m.probe_layout =
      make_layout(device, "snow.probe", {Slot::uniform, Slot::storage_read, Slot::storage},
                  wgpu::ShaderStage::Compute);
  const wgpu::BindGroupLayout none =
      make_layout(device, "snow.probe.none", {}, wgpu::ShaderStage::Compute);
  const wgpu::PipelineLayout layout =
      make_pipeline_layout(device, "snow.probe", {m.probe_layout, none, none, m.layout});
  const wgpu::ShaderModule module = gpu::create_shader(device, ge::wgsl::snow_probe, "snow.probe");
  for (const char* entry : {"raw", "columns"}) {
    wgpu::ComputePipelineDescriptor desc{};
    desc.label = entry;
    desc.layout = layout;
    desc.compute.module = module;
    desc.compute.entryPoint = entry;
    (std::string(entry) == "raw" ? m.probe_raw : m.probe_columns) =
        device.device.CreateComputePipeline(&desc);
  }
}

// One dispatch of `pipeline` over `count` invocations, `words` words of output read back.
std::vector<uint32_t> run_probe(gpu::Device& device, SnowMirror& m,
                                const wgpu::ComputePipeline& pipeline, const int32_t rect[4],
                                std::span<const float> xz, uint32_t count, uint64_t words) {
  std::vector<uint32_t> out(words, 0);
  if (!count) return out;
  const wgpu::Buffer params = make_buffer(device, "snow.probe.params", 16, BU::Uniform);
  device.queue.WriteBuffer(params, 0, rect, 16);
  const uint64_t xz_bytes = std::max<uint64_t>(xz.size_bytes(), 8);
  const wgpu::Buffer points = make_buffer(device, "snow.probe.xz", xz_bytes, BU::Storage);
  if (!xz.empty()) device.queue.WriteBuffer(points, 0, xz.data(), xz.size_bytes());
  const wgpu::Buffer result =
      make_buffer(device, "snow.probe.out", words * 4, BU::Storage | BU::CopySrc);
  wgpu::BufferDescriptor rd{};
  rd.label = "snow.probe.readback";
  rd.size = words * 4;
  rd.usage = BU::MapRead | BU::CopyDst;
  const wgpu::Buffer readback = device.device.CreateBuffer(&rd);
  const wgpu::BindGroup group =
      make_group(device, "snow.probe", m.probe_layout, {params, points, result});
  const wgpu::BindGroupLayout none =
      make_layout(device, "snow.probe.none", {}, wgpu::ShaderStage::Compute);
  const wgpu::BindGroup empty = make_group(device, "snow.probe.none", none, {});
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, group);
  pass.SetBindGroup(1, empty);
  pass.SetBindGroup(2, empty);
  pass.SetBindGroup(3, m.group);
  pass.DispatchWorkgroups((count + 63) / 64);
  pass.End();
  encoder.CopyBufferToBuffer(result, 0, readback, 0, words * 4);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  gpu::wait(device,
            readback.MapAsync(wgpu::MapMode::Read, 0, words * 4, wgpu::CallbackMode::WaitAnyOnly,
                              [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  if (const void* p = readback.GetConstMappedRange(0, words * 4)) {
    std::memcpy(out.data(), p, words * 4);
  }
  readback.Unmap();
  return out;
}

}  // namespace

std::vector<SnowRaw> read_snow_raw(gpu::Device& device, SnowMirror& m, int c0, int r0, int w,
                                   int h) {
  make_probe(device, m);
  std::vector<SnowRaw> out(static_cast<size_t>(std::max(w, 0)) * std::max(h, 0));
  // A band of rows a dispatch, inside the workgroup count's limit.
  const int band = std::max(1, (65535 * 64) / std::max(w, 1));
  for (int r = 0; r < h; r += band) {
    const int rows = std::min(band, h - r);
    const int32_t rect[4] = {c0, r0 + r, w, rows};
    const auto count = static_cast<uint32_t>(w * rows);
    const std::vector<uint32_t> words =
        run_probe(device, m, m.probe_raw, rect, {}, count, uint64_t{count} * 5);
    for (uint32_t k = 0; k < count; k++) {
      SnowRaw& s = out[static_cast<size_t>(r) * w + k];
      const size_t o = size_t{5} * k;
      s = {words[o], words[o + 1], words[o + 2], words[o + 3], words[o + 4]};
    }
  }
  return out;
}

std::vector<SnowColumnRead> read_snow_columns(gpu::Device& device, SnowMirror& m,
                                              std::span<const float> xz) {
  make_probe(device, m);
  const auto n = static_cast<uint32_t>(xz.size() / 2);
  std::vector<SnowColumnRead> out(n);
  const uint32_t chunk = 65535u * 64u;
  for (uint32_t at = 0; at < n; at += chunk) {
    const uint32_t count = std::min(chunk, n - at);
    const int32_t rect[4] = {0, 0, static_cast<int32_t>(count), 1};
    const std::vector<uint32_t> words =
        run_probe(device, m, m.probe_columns, rect, xz.subspan(size_t{at} * 2, size_t{count} * 2),
                  count, uint64_t{count} * 6);
    static_assert(sizeof(SnowColumnRead) == 24);
    std::memcpy(&out[at], words.data(), size_t{count} * 24);
  }
  return out;
}

}  // namespace ge::render
