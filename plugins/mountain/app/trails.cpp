// The trail map's targets, its stamp pipeline, the history and its buckets, the window's moves
// and the two passes.
//
// ge:summary The trail map's targets, stamp pipeline, history, window moves and passes
// ge:keywords trails stamp pipeline max-blend history buckets window redraw passes
// ge:skill mountain render
// ge:test tests/mountain_trails_test.cpp
#include "mountain/app/trails.hpp"

#include <algorithm>
#include <cmath>

#include "mountain/app/trails.wgsl.hpp"

namespace ge::mountain::app {

namespace {

// The history's buckets are this many metres a side.
constexpr double kBucket = 32;
// The furthest a stamp's quad reaches from its centre, m: a capsule's length (a move of up to
// skier/app/tracks.hpp's 6 m) and a body's berm. A redraw reads the buckets this much round the
// window.
constexpr double kStampReach = 8;

wgpu::RenderPassEncoder open_pass(wgpu::CommandEncoder& encoder, const gpu::Target& target,
                                  bool clear, render::FrameRecorder* rec, const char* name) {
  wgpu::RenderPassColorAttachment colour{};
  colour.view = target.view;
  colour.loadOp = clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
  colour.storeOp = wgpu::StoreOp::Store;
  colour.clearValue = {0, 0, 0, 0};
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  if (rec) return rec->render(encoder, desc, name);
  desc.label = name;
  return encoder.BeginRenderPass(&desc);
}

// One map's pass: its stamps are `count` from `first` in the buffer.
void stamp_pass(Trails& t, const gpu::PipelineSet& pipelines, wgpu::CommandEncoder& encoder,
                render::FrameRecorder* rec, int map, uint32_t first, uint32_t count) {
  const bool fine = map == 0;
  bool& clear = fine ? t.fine_clear : t.coarse_clear;
  const char* name = fine ? "trails.fine" : "trails.coarse";
  if (count == 0 && !clear) {
    // Nothing to stamp: an empty pass keeps the frame's timing row without loading and storing
    // the whole map (a render pass with no draws still moves every texel on a tiler).
    if (rec) rec->compute(encoder, name).End();
    return;
  }
  wgpu::RenderPassEncoder pass = open_pass(encoder, fine ? t.fine : t.coarse, clear, rec, name);
  clear = false;
  if (count > 0) {
    pass.SetPipeline(pipelines.render(t.p_stamp));
    pass.SetBindGroup(0, t.groups[map]);
    pass.Draw(6, count, 0, first);
    if (rec) ++rec->current().draws;
  }
  pass.End();
}

size_t bucket_of(const Trails& t, double x, double z) {
  const int c = std::clamp(static_cast<int>(std::floor((x - t.coarse_rect.x0) / kBucket)), 0,
                           t.bucket_cols - 1);
  const int r = std::clamp(static_cast<int>(std::floor((z - t.coarse_rect.z0) / kBucket)), 0,
                           t.bucket_rows - 1);
  return static_cast<size_t>(r) * static_cast<size_t>(t.bucket_cols) + static_cast<size_t>(c);
}

void write_rect(gpu::Device& device, const wgpu::Buffer& buffer, const TrailRect& r) {
  device.queue.WriteBuffer(buffer, 0, &r, sizeof r);
}

}  // namespace

TrailWindow trail_window_at(double span, double x, double z) {
  const double grid = span / 8;
  return {std::round((x - span / 2) / grid) * grid, std::round((z - span / 2) / grid) * grid};
}

bool trail_window_moves(const TrailWindow& w, double span, double x, double z) {
  return std::fabs(x - (w.x0 + span / 2)) > span / 4 || std::fabs(z - (w.z0 + span / 2)) > span / 4;
}

bool trail_stamp_meets(const TrailStamp& s, double x0, double z0, double span) {
  // The quad's reach, generously: the berm past the groove and a coarse texel.
  const double reach = 2.0 * s.half_width + 0.1;
  return std::min(s.ax, s.bx) - reach <= x0 + span && std::max(s.ax, s.bx) + reach >= x0 &&
         std::min(s.az, s.bz) - reach <= z0 + span && std::max(s.az, s.bz) + reach >= z0;
}

TrailRect fine_rect(const Trails& t) {
  const double span = t.options.fine_span;
  return {static_cast<float>(t.window.x0), static_cast<float>(t.window.z0),
          static_cast<float>(span), static_cast<float>(span / t.options.fine_size)};
}

void create_trails(gpu::Device& device, gpu::PipelineSet& pipelines, Trails& t,
                   const mapgen::Level& level, const TrailOptions& options) {
  namespace r = ge::render;
  t.options = options;
  const auto& g = level.ground;
  const double extent = std::max(g.cols - 1, g.rows - 1) * g.cell;
  t.coarse_rect = {static_cast<float>(g.origin_x), static_cast<float>(g.origin_z),
                   static_cast<float>(extent), static_cast<float>(extent / options.coarse_size)};
  constexpr auto kFormat = wgpu::TextureFormat::RGBA8Unorm;
  t.fine = gpu::create_target(device, "trails.fine", options.fine_size, options.fine_size, kFormat);
  t.coarse = gpu::create_target(device, "trails.coarse", options.coarse_size, options.coarse_size,
                                kFormat);
  t.stamps =
      r::make_buffer(device, "trails.stamps", uint64_t{options.capacity} * sizeof(TrailStamp),
                     wgpu::BufferUsage::Storage);
  t.rects[0] =
      r::make_buffer(device, "trails.fine.rect", sizeof(TrailRect), wgpu::BufferUsage::Uniform);
  t.rects[1] =
      r::make_buffer(device, "trails.coarse.rect", sizeof(TrailRect), wgpu::BufferUsage::Uniform);
  write_rect(device, t.rects[1], t.coarse_rect);
  write_rect(device, t.rects[0], fine_rect(t));
  t.layout = r::make_layout(device, "trails", {r::Slot::uniform, r::Slot::storage_read},
                            wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
  for (int k = 0; k < 2; ++k) {
    t.groups[k] = r::make_group(device, k == 0 ? "trails.fine" : "trails.coarse", t.layout,
                                {t.rects[k], t.stamps});
  }
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::trails, "trails");
  wgpu::BlendState blend{};
  blend.color.operation = wgpu::BlendOperation::Max;
  blend.color.srcFactor = wgpu::BlendFactor::One;
  blend.color.dstFactor = wgpu::BlendFactor::One;
  blend.alpha = blend.color;
  wgpu::ColorTargetState colour{};
  colour.format = kFormat;
  colour.blend = &blend;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "trails.stamp";
  desc.layout = r::make_pipeline_layout(device, "trails", {t.layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.fragment = &fragment;
  t.p_stamp = pipelines.add_render(device, desc);
  t.bucket_cols = static_cast<int>(std::ceil(extent / kBucket)) + 1;
  t.bucket_rows = t.bucket_cols;
  t.buckets.assign(static_cast<size_t>(t.bucket_cols) * static_cast<size_t>(t.bucket_rows), {});
}

void add_trail_stamps(Trails& t, std::span<const TrailStamp> stamps) {
  for (const TrailStamp& s : stamps) {
    const auto i = static_cast<uint32_t>(t.history.size());
    t.history.push_back(s);
    t.buckets[bucket_of(t, 0.5 * (s.ax + s.bx), 0.5 * (s.az + s.bz))].push_back(i);
    t.pending.push_back(s);
  }
}

void clear_trails(Trails& t) {
  t.history.clear();
  for (auto& b : t.buckets) b.clear();
  t.pending.clear();
  t.placed = false;
  t.fine_clear = t.coarse_clear = true;
}

void prepare_trails(gpu::Device& device, const gpu::PipelineSet& pipelines, Trails& t, double eye_x,
                    double eye_z) {
  const double span = t.options.fine_span;
  const bool redraw = !t.placed || trail_window_moves(t.window, span, eye_x, eye_z);
  // The fine map's stamps: on a redraw every stamp of the history inside the new window, else
  // this frame's inside it; the coarse map's, this frame's.
  std::vector<TrailStamp> fine;
  if (redraw) {
    t.window = trail_window_at(span, eye_x, eye_z);
    t.placed = true;
    t.fine_clear = true;
    write_rect(device, t.rects[0], fine_rect(t));
    const size_t lo = bucket_of(t, t.window.x0 - kStampReach, t.window.z0 - kStampReach);
    const size_t hi =
        bucket_of(t, t.window.x0 + span + kStampReach, t.window.z0 + span + kStampReach);
    const size_t cols = static_cast<size_t>(t.bucket_cols);
    for (size_t r = lo / cols; r <= hi / cols; ++r) {
      for (size_t c = lo % cols; c <= hi % cols; ++c) {
        for (uint32_t i : t.buckets[r * cols + c]) {
          const TrailStamp& s = t.history[i];
          if (trail_stamp_meets(s, t.window.x0, t.window.z0, span)) fine.push_back(s);
        }
      }
    }
  } else {
    for (const TrailStamp& s : t.pending) {
      if (trail_stamp_meets(s, t.window.x0, t.window.z0, span)) fine.push_back(s);
    }
  }
  size_t f = 0, c = 0;  // drawn so far of `fine` and of `pending`
  const size_t cap = t.options.capacity;
  // What will not fit in the frame's draw goes in command buffers of its own, in order.
  while ((fine.size() - f) + (t.pending.size() - c) > cap) {
    const size_t nf = std::min(fine.size() - f, cap);
    const size_t nc = std::min(t.pending.size() - c, cap - nf);
    t.staged.assign(fine.begin() + static_cast<std::ptrdiff_t>(f),
                    fine.begin() + static_cast<std::ptrdiff_t>(f + nf));
    t.staged.insert(t.staged.end(), t.pending.begin() + static_cast<std::ptrdiff_t>(c),
                    t.pending.begin() + static_cast<std::ptrdiff_t>(c + nc));
    device.queue.WriteBuffer(t.stamps, 0, t.staged.data(), t.staged.size() * sizeof(TrailStamp));
    wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
    stamp_pass(t, pipelines, encoder, nullptr, 0, 0, static_cast<uint32_t>(nf));
    stamp_pass(t, pipelines, encoder, nullptr, 1, static_cast<uint32_t>(nf),
               static_cast<uint32_t>(nc));
    wgpu::CommandBuffer commands = encoder.Finish();
    device.queue.Submit(1, &commands);
    f += nf;
    c += nc;
  }
  t.staged.assign(fine.begin() + static_cast<std::ptrdiff_t>(f), fine.end());
  t.fine_count = static_cast<uint32_t>(t.staged.size());
  t.staged.insert(t.staged.end(), t.pending.begin() + static_cast<std::ptrdiff_t>(c),
                  t.pending.end());
  t.coarse_count = static_cast<uint32_t>(t.staged.size()) - t.fine_count;
  if (!t.staged.empty()) {
    device.queue.WriteBuffer(t.stamps, 0, t.staged.data(), t.staged.size() * sizeof(TrailStamp));
  }
  t.pending.clear();
}

void encode_trails(Trails& t, const gpu::PipelineSet& pipelines, render::FrameRecorder& rec,
                   wgpu::CommandEncoder& encoder) {
  stamp_pass(t, pipelines, encoder, &rec, 0, 0, t.fine_count);
  stamp_pass(t, pipelines, encoder, &rec, 1, t.fine_count, t.coarse_count);
  t.fine_count = t.coarse_count = 0;
}

}  // namespace ge::mountain::app
