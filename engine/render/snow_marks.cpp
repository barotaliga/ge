// The fine marks' targets, their stamp pipeline, the history and its buckets, the window's moves,
// the two passes, and the stamps a layer's imprint records leave.
//
// ge:summary The fine marks' targets, stamp pipeline, history, window moves, passes and records
// ge:keywords snow-marks stamp pipeline max-blend history buckets window redraw passes records
// ge:skill render snow
// ge:test tests/render_snow_marks_test.cpp
#include "render/snow_marks.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "render/snow_marks.wgsl.hpp"

namespace ge::render {

namespace {

// The history's buckets are this many metres a side.
constexpr double kBucket = 32;
// The furthest a stamp's quad reaches from its centre, m: a capsule's length (a ski's move of up
// to 6 m between frames) and a body's berm. A redraw reads the buckets this much round the
// window.
constexpr double kStampReach = 8;

wgpu::RenderPassEncoder open_pass(wgpu::CommandEncoder& encoder, const gpu::Target& target,
                                  bool clear, FrameRecorder* rec, const char* name) {
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
void stamp_pass(SnowMarks& t, const gpu::PipelineSet& pipelines, wgpu::CommandEncoder& encoder,
                FrameRecorder* rec, int map, uint32_t first, uint32_t count) {
  const bool fine = map == 0;
  bool& clear = fine ? t.fine_clear : t.coarse_clear;
  const char* name = t.pass_names[fine ? 0 : 1].c_str();
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

size_t bucket_of(const SnowMarks& t, double x, double z) {
  const int c = std::clamp(static_cast<int>(std::floor((x - t.coarse_rect.x0) / kBucket)), 0,
                           t.bucket_cols - 1);
  const int r = std::clamp(static_cast<int>(std::floor((z - t.coarse_rect.z0) / kBucket)), 0,
                           t.bucket_rows - 1);
  return static_cast<size_t>(r) * static_cast<size_t>(t.bucket_cols) + static_cast<size_t>(c);
}

void write_rect(gpu::Device& device, const wgpu::Buffer& buffer, const MarkRect& r) {
  device.queue.WriteBuffer(buffer, 0, &r, sizeof r);
}

}  // namespace

MarkWindow mark_window_at(double span, double x, double z) {
  const double grid = span / 8;
  return {std::round((x - span / 2) / grid) * grid, std::round((z - span / 2) / grid) * grid};
}

bool mark_window_moves(const MarkWindow& w, double span, double x, double z) {
  return std::fabs(x - (w.x0 + span / 2)) > span / 4 || std::fabs(z - (w.z0 + span / 2)) > span / 4;
}

bool mark_stamp_meets(const MarkStamp& s, double x0, double z0, double span) {
  // The quad's reach, generously: the berm past the groove and a coarse texel.
  const double reach = 2.0 * s.half_width + 0.1;
  return std::min(s.ax, s.bx) - reach <= x0 + span && std::max(s.ax, s.bx) + reach >= x0 &&
         std::min(s.az, s.bz) - reach <= z0 + span && std::max(s.az, s.bz) + reach >= z0;
}

MarkRect fine_rect(const SnowMarks& t) {
  const double span = t.options.fine_span;
  return {static_cast<float>(t.window.x0), static_cast<float>(t.window.z0),
          static_cast<float>(span), static_cast<float>(span / t.options.fine_size)};
}

void marks_from_records(std::span<const snow::ImprintRecord> records, std::vector<MarkStamp>& out) {
  // The berm's snow is loose: thrown aside, it lies at about this density, kg/m3.
  constexpr double kBermDensity = 250;
  for (const snow::ImprintRecord& rec : records) {
    const snow::Shape& sh = rec.shape;
    if (sh.half_width <= 0 || rec.sink <= 0) continue;
    MarkStamp m{};
    m.half_width = static_cast<float>(sh.half_width);
    m.depth = static_cast<float>(std::min<double>(rec.sink, kMarkDepth));
    double length = 0;  // the outline's length along its axis, m
    switch (sh.kind) {
      case snow::ShapeKind::capsule:
      case snow::ShapeKind::disc: {
        m.shape = MarkShape::capsule;
        m.ax = static_cast<float>(sh.ax);
        m.az = static_cast<float>(sh.az);
        m.bx = static_cast<float>(sh.bx);
        m.bz = static_cast<float>(sh.bz);
        length = std::hypot(sh.bx - sh.ax, sh.bz - sh.az);
        break;
      }
      case snow::ShapeKind::box:
      case snow::ShapeKind::ellipse: {
        // Along the heading: 0 = +z, clockwise from above.
        m.shape = sh.kind == snow::ShapeKind::box ? MarkShape::box : MarkShape::ellipse;
        const double dx = std::sin(sh.heading) * sh.half_length;
        const double dz = std::cos(sh.heading) * sh.half_length;
        m.ax = static_cast<float>(sh.ax - dx);
        m.az = static_cast<float>(sh.az - dz);
        m.bx = static_cast<float>(sh.ax + dx);
        m.bz = static_cast<float>(sh.az + dz);
        length = 2 * sh.half_length;
        break;
      }
    }
    // The snow thrown aside, heaped along both sides of the outline in a bump of the berm's reach
    // (its cross-section two thirds of its height times its width).
    const double rim = 2 * length + 2 * std::numbers::pi * sh.half_width;
    const double bump = rim * kMarkBermReach * sh.half_width * (2.0 / 3.0);
    const double berm = bump > 0 ? rec.thrown / kBermDensity / bump : 0;
    m.berm = static_cast<float>(std::min(berm, kMarkBerm));
    switch (rec.motion) {
      case snow::Motion::slide:
        m.wall = 2;  // a ski's round groove
        break;
      case snow::Motion::plough:
      case snow::Motion::dig:
        m.wall = 3;
        break;
      case snow::Motion::roll:
        m.wall = 6;  // a tyre's flat floor
        break;
      case snow::Motion::step:
        m.wall = 10;  // a boot's square print
        break;
    }
    out.push_back(m);
  }
}

void create_snow_marks(gpu::Device& device, gpu::PipelineSet& pipelines, SnowMarks& t, double x0,
                       double z0, double extent, const MarkOptions& options) {
  t.options = options;
  const std::string& l = options.label;
  t.pass_names[0] = l + ".fine";
  t.pass_names[1] = l + ".coarse";
  t.coarse_rect = {static_cast<float>(x0), static_cast<float>(z0), static_cast<float>(extent),
                   static_cast<float>(extent / options.coarse_size)};
  constexpr auto kFormat = wgpu::TextureFormat::RGBA8Unorm;
  t.fine =
      gpu::create_target(device, t.pass_names[0], options.fine_size, options.fine_size, kFormat);
  t.coarse = gpu::create_target(device, t.pass_names[1], options.coarse_size, options.coarse_size,
                                kFormat);
  t.stamps = make_buffer(device, l + ".stamps", uint64_t{options.capacity} * sizeof(MarkStamp),
                         wgpu::BufferUsage::Storage);
  t.rects[0] = make_buffer(device, l + ".fine.rect", sizeof(MarkRect), wgpu::BufferUsage::Uniform);
  t.rects[1] =
      make_buffer(device, l + ".coarse.rect", sizeof(MarkRect), wgpu::BufferUsage::Uniform);
  write_rect(device, t.rects[1], t.coarse_rect);
  write_rect(device, t.rects[0], fine_rect(t));
  t.layout = make_layout(device, l, {Slot::uniform, Slot::storage_read},
                         wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
  for (int k = 0; k < 2; ++k) {
    t.groups[k] = make_group(device, t.pass_names[k], t.layout, {t.rects[k], t.stamps});
  }
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::snow_marks, l);
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
  const std::string stamp_label = l + ".stamp";
  desc.label = stamp_label.c_str();
  desc.layout = make_pipeline_layout(device, l, {t.layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.fragment = &fragment;
  t.p_stamp = pipelines.add_render(device, desc);
  t.bucket_cols = static_cast<int>(std::ceil(extent / kBucket)) + 1;
  t.bucket_rows = t.bucket_cols;
  t.buckets.assign(static_cast<size_t>(t.bucket_cols) * static_cast<size_t>(t.bucket_rows), {});
}

void add_mark_stamps(SnowMarks& t, std::span<const MarkStamp> stamps) {
  for (const MarkStamp& s : stamps) {
    const auto i = static_cast<uint32_t>(t.history.size());
    t.history.push_back(s);
    t.buckets[bucket_of(t, 0.5 * (s.ax + s.bx), 0.5 * (s.az + s.bz))].push_back(i);
    t.pending.push_back(s);
  }
}

void clear_snow_marks(SnowMarks& t) {
  t.history.clear();
  for (auto& b : t.buckets) b.clear();
  t.pending.clear();
  t.placed = false;
  t.fine_clear = t.coarse_clear = true;
}

void prepare_snow_marks(gpu::Device& device, const gpu::PipelineSet& pipelines, SnowMarks& t,
                        double eye_x, double eye_z) {
  const double span = t.options.fine_span;
  const bool redraw = !t.placed || mark_window_moves(t.window, span, eye_x, eye_z);
  // The fine map's stamps: on a redraw every stamp of the history inside the new window, else
  // this frame's inside it; the coarse map's, this frame's.
  std::vector<MarkStamp> fine;
  if (redraw) {
    t.window = mark_window_at(span, eye_x, eye_z);
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
          const MarkStamp& s = t.history[i];
          if (mark_stamp_meets(s, t.window.x0, t.window.z0, span)) fine.push_back(s);
        }
      }
    }
  } else {
    for (const MarkStamp& s : t.pending) {
      if (mark_stamp_meets(s, t.window.x0, t.window.z0, span)) fine.push_back(s);
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
    device.queue.WriteBuffer(t.stamps, 0, t.staged.data(), t.staged.size() * sizeof(MarkStamp));
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
    device.queue.WriteBuffer(t.stamps, 0, t.staged.data(), t.staged.size() * sizeof(MarkStamp));
  }
  t.pending.clear();
}

void encode_snow_marks(SnowMarks& t, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                       wgpu::CommandEncoder& encoder) {
  stamp_pass(t, pipelines, encoder, &rec, 0, 0, t.fine_count);
  stamp_pass(t, pipelines, encoder, &rec, 1, t.fine_count, t.coarse_count);
  t.fine_count = t.coarse_count = 0;
}

}  // namespace ge::render
