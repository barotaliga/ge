// The texture upscaler on the GPU (app/upscaler_gpu.hpp).
//
// ge:summary The GPU upscaler's pipelines, packed weights, recorded dispatches and readback
// ge:keywords upscaler gpu pipelines bind-group uniform-ring dispatch record readback weights pack
// ge:skill classicenhance gpu
// ge:test tests/classicenhance_upscaler_gpu_test.cpp
#include "classicenhance/app/upscaler_gpu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include "classicenhance/app/upscaler.wgsl.hpp"
#include "gpu/pipeline.hpp"

namespace ge::classicenhance {

namespace {

using BU = wgpu::BufferUsage;

enum Kernel : uint8_t {
  k_gemm,
  k_depthwise,
  k_layer_norm,
  k_window,
  k_column_sum,
  k_channel_scores,
  k_softmax_rows,
  k_channel_apply,
  k_gate_mix,
  k_multiply,
  k_shuffle,
  k_count,
};
constexpr std::array<const char*, k_count> kEntry = {
    "gemm",         "depthwise",     "layer_norm", "window",   "column_sum", "channel_scores",
    "softmax_rows", "channel_apply", "gate_mix",   "multiply", "shuffle"};

// upscaler.wgsl's Params.
struct Params {
  uint32_t rows = 0, k = 0, n = 0, flags = 0;
  uint32_t xs = 0, xo = 0, ys = 0, yo = 0;
  uint32_t h = 0, w = 0, cin = 0, a0 = 0;
  uint32_t a1 = 0, a2 = 0, a3 = 0, a4 = 0;
  float f0 = 0, f1 = 0, f2 = 0, f3 = 0;
};
constexpr uint32_t kGelu = 1, kAdd = 2, kLeaky = 4, kConv = 8;

// A range of a buffer a binding sees; size 0 is the whole buffer from `offset`.
struct Ref {
  wgpu::Buffer buffer;
  uint64_t offset = 0, size = 0;
};

struct Op {
  Kernel kernel = k_gemm;
  Params p;
  Ref src, wts, bias, dst, aux;
  uint32_t gx = 1, gy = 1;
  bool copy = false;  // a buffer copy: src's range to dst's
};

struct LinearRef {
  Ref w, b;
  int in = 0, out = 0;
};

struct BlockRefs {
  Ref norm1_w, norm1_b, norm2_w, norm2_b;
  LinearRef qkv, proj, channel1, channel2, spatial1, spatial2, fc1, fc2;
  std::array<Ref, 2> bias;
  Ref temperature;
  Ref dw_w, dw_b, gate_norm_w, gate_norm_b, gate_w, gate_b;
};

}  // namespace

struct GpuUpscaler::Impl {
  gpu::Device* device = nullptr;
  const Upscaler* net = nullptr;
  wgpu::BindGroupLayout layout;
  gpu::PipelineSet pipelines;
  std::array<uint32_t, k_count> pipeline{};
  wgpu::Buffer weights, dummy;
  uint64_t align = 256, uniform_align = 256;
  LinearRef first, after_body, before_up, last;
  std::vector<LinearRef> up, group_conv;
  Ref before_w, before_b, norm_w, norm_b;
  std::vector<std::vector<BlockRefs>> blocks;
  std::vector<Op> ops;

  // ---- weights ----

  std::vector<float> packed;
  Ref put(const std::vector<float>& v) {
    const uint64_t step = align / 4;
    const uint64_t at = (packed.size() + step - 1) / step * step;
    packed.resize(size_t(at), 0.0f);
    packed.insert(packed.end(), v.begin(), v.end());
    return {nullptr, at * 4, std::max<uint64_t>(4, uint64_t(v.size()) * 4)};
  }
  LinearRef put(const net::Linear& l) { return {put(l.wt), put(l.b), l.in, l.out}; }
  LinearRef put(const net::Conv3& c) { return {put(c.wt), put(c.b), c.in, c.out}; }

  // ---- recording ----

  wgpu::Buffer buffer(uint64_t floats, const char* label) {
    wgpu::BufferDescriptor d{};
    d.label = label;
    d.size = std::max<uint64_t>(16, floats * 4);
    d.usage = BU::Storage | BU::CopyDst | BU::CopySrc;
    return device->device.CreateBuffer(&d);
  }
  static Ref whole(const wgpu::Buffer& b) { return {b, 0, 0}; }

  void grid(Op& op, uint64_t items) {
    const uint64_t groups = std::max<uint64_t>(1, (items + 255) / 256);
    op.gx = uint32_t(std::min<uint64_t>(groups, 32768));
    op.gy = uint32_t((groups + op.gx - 1) / op.gx);
  }

  // y[r] (stride ys, from yo) = x[r] (stride xs, from xo) · W + b, or the 3 × 3 convolution of an
  // h × w map when `conv`.
  void gemm(const LinearRef& l, const Ref& x, uint32_t xs, uint32_t xo, const Ref& y, uint32_t ys,
            uint32_t yo, uint32_t rows, uint32_t flags, int h = 0, int w = 0) {
    Op op;
    op.kernel = k_gemm;
    const bool conv = (flags & kConv) != 0;
    op.p.rows = rows;
    op.p.k = uint32_t(conv ? 9 * l.in : l.in);
    op.p.n = uint32_t(l.out);
    op.p.flags = flags;
    op.p.xs = xs;
    op.p.xo = xo;
    op.p.ys = ys;
    op.p.yo = yo;
    op.p.h = uint32_t(h);
    op.p.w = uint32_t(w);
    op.p.cin = uint32_t(l.in);
    op.src = x;
    op.wts = weight(l.w);
    op.bias = weight(l.b);
    op.dst = y;
    op.gx = (op.p.n + 63) / 64;
    op.gy = (rows + 63) / 64;
    ops.push_back(op);
  }
  Ref weight(Ref r) const {
    r.buffer = weights;
    return r;
  }
  void conv(const LinearRef& l, const Ref& x, const Ref& y, uint32_t flags, int h, int w) {
    gemm(l, x, uint32_t(l.in), 0, y, uint32_t(l.out), 0, uint32_t(h * w), flags | kConv, h, w);
  }
  void layer_norm(const Ref& w, const Ref& b, const Ref& x, uint32_t xs, uint32_t xo, const Ref& y,
                  uint32_t rows, uint32_t n) {
    Op op;
    op.kernel = k_layer_norm;
    op.p.rows = rows;
    op.p.n = n;
    op.p.xs = xs;
    op.p.xo = xo;
    op.p.ys = n;
    op.src = x;
    op.wts = weight(w);
    op.bias = weight(b);
    op.dst = y;
    grid(op, rows);
    ops.push_back(op);
  }
  void depthwise(const Ref& w, const Ref& b, const Ref& x, uint32_t xs, uint32_t xo, const Ref& y,
                 uint32_t n, int h, int wd, bool gelu) {
    Op op;
    op.kernel = k_depthwise;
    op.p.rows = uint32_t(h * wd);
    op.p.n = n;
    op.p.flags = gelu ? kGelu : 0;
    op.p.xs = xs;
    op.p.xo = xo;
    op.p.ys = n;
    op.p.h = uint32_t(h);
    op.p.w = uint32_t(wd);
    op.src = x;
    op.wts = weight(w);
    op.bias = weight(b);
    op.dst = y;
    grid(op, uint64_t(op.p.rows) * n);
    ops.push_back(op);
  }
  void column_sum(const Ref& x, uint32_t xs, uint32_t xo, uint32_t rows, uint32_t n, bool square,
                  float scale, const Ref& y) {
    Op op;
    op.kernel = k_column_sum;
    op.p.rows = rows;
    op.p.n = n;
    op.p.flags = square ? 1 : 0;
    op.p.xs = xs;
    op.p.xo = xo;
    op.p.f0 = scale;
    op.src = x;
    op.dst = y;
    op.gx = n;
    ops.push_back(op);
  }
  void elementwise(Kernel k, Params p, const Ref& src, const Ref& wts, const Ref& bias,
                   const Ref& dst, const Ref& aux, uint64_t items) {
    Op op;
    op.kernel = k;
    op.p = p;
    op.src = src;
    op.wts = wts;
    op.bias = bias;
    op.dst = dst;
    op.aux = aux;
    grid(op, items);
    ops.push_back(op);
  }
  void copy(const Ref& from, const Ref& to) {
    Op op;
    op.copy = true;
    op.src = from;
    op.dst = to;
    ops.push_back(op);
  }

  bool submit(std::string& error);
};

GpuUpscaler::GpuUpscaler(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
GpuUpscaler::~GpuUpscaler() = default;

std::string GpuUpscaler::name() const { return "gpu"; }

std::unique_ptr<GpuUpscaler> GpuUpscaler::create(gpu::Device& device, const Upscaler& net,
                                                 std::string& error) {
  auto impl = std::make_unique<Impl>();
  impl->device = &device;
  impl->net = &net;
  impl->align = std::max<uint64_t>(4, device.limits.minStorageBufferOffsetAlignment);
  impl->uniform_align =
      std::max<uint64_t>(sizeof(Params), device.limits.minUniformBufferOffsetAlignment);

  std::array<wgpu::BindGroupLayoutEntry, 6> entries{};
  for (uint32_t i = 0; i < 6; i++) {
    entries[i].binding = i;
    entries[i].visibility = wgpu::ShaderStage::Compute;
    entries[i].buffer.type = i == 0   ? wgpu::BufferBindingType::Uniform
                             : i == 4 ? wgpu::BufferBindingType::Storage
                                      : wgpu::BufferBindingType::ReadOnlyStorage;
  }
  wgpu::BindGroupLayoutDescriptor ld{};
  ld.label = "upscaler";
  ld.entryCount = entries.size();
  ld.entries = entries.data();
  impl->layout = device.device.CreateBindGroupLayout(&ld);
  wgpu::PipelineLayoutDescriptor pd{};
  pd.label = "upscaler";
  pd.bindGroupLayoutCount = 1;
  pd.bindGroupLayouts = &impl->layout;
  const wgpu::PipelineLayout layout = device.device.CreatePipelineLayout(&pd);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::upscaler, "upscaler");
  for (uint32_t k = 0; k < k_count; k++) {
    wgpu::ComputePipelineDescriptor desc{};
    const std::string label = std::string("upscaler.") + kEntry[k];
    desc.label = label.c_str();
    desc.layout = layout;
    desc.compute.module = module;
    desc.compute.entryPoint = kEntry[k];
    impl->pipeline[k] = impl->pipelines.add_compute(device, desc);
  }
  if (!impl->pipelines.finish(device)) {
    error = "the upscaler's compute pipelines failed to build on " + gpu::adapter_line(device) +
            " (the error lines above name each); run on the CPU";
    return nullptr;
  }

  Impl& m = *impl;
  m.first = m.put(net.first);
  m.before_w = m.put(net.before.w);
  m.before_b = m.put(net.before.b);
  for (const UpscalerGroup& g : net.groups) {
    auto& rows = m.blocks.emplace_back();
    for (const UpscalerBlock& k : g.blocks) {
      BlockRefs& r = rows.emplace_back();
      r.norm1_w = m.put(k.norm1.w);
      r.norm1_b = m.put(k.norm1.b);
      r.norm2_w = m.put(k.norm2.w);
      r.norm2_b = m.put(k.norm2.b);
      r.qkv = m.put(k.qkv);
      r.proj = m.put(k.proj);
      for (size_t b = 0; b < 2; b++) r.bias[b] = m.put(k.bias[b]);
      r.temperature = m.put(k.temperature);
      r.dw_w = m.put(k.dwconv.w);
      r.dw_b = m.put(k.dwconv.b);
      r.channel1 = m.put(k.channel1);
      r.channel2 = m.put(k.channel2);
      r.spatial1 = m.put(k.spatial1);
      r.spatial2 = m.put(k.spatial2);
      r.fc1 = m.put(k.fc1);
      r.fc2 = m.put(k.fc2);
      r.gate_norm_w = m.put(k.gate_norm.w);
      r.gate_norm_b = m.put(k.gate_norm.b);
      r.gate_w = m.put(k.gate_conv.w);
      r.gate_b = m.put(k.gate_conv.b);
    }
    m.group_conv.push_back(m.put(g.conv));
  }
  m.norm_w = m.put(net.norm.w);
  m.norm_b = m.put(net.norm.b);
  m.after_body = m.put(net.after_body);
  m.before_up = m.put(net.before_up);
  for (const net::Conv3& c : net.up) m.up.push_back(m.put(c));
  m.last = m.put(net.last);
  const uint64_t bytes = uint64_t(m.packed.size()) * 4;
  if (bytes > device.limits.maxBufferSize) {
    error = "the upscaler's weights (" + std::to_string(bytes >> 20) + " MiB) exceed " +
            gpu::adapter_line(device) + "'s largest buffer; run on the CPU";
    return nullptr;
  }
  wgpu::BufferDescriptor wd{};
  wd.label = "upscaler.weights";
  wd.size = bytes;
  wd.usage = BU::Storage | BU::CopyDst;
  m.weights = device.device.CreateBuffer(&wd);
  device.queue.WriteBuffer(m.weights, 0, m.packed.data(), bytes);
  m.packed = {};
  m.dummy = m.buffer(64, "upscaler.dummy");
  return std::unique_ptr<GpuUpscaler>(new GpuUpscaler(std::move(impl)));
}

bool GpuUpscaler::Impl::submit(std::string& error) {
  // Every dispatch's parameters in one uniform buffer, at `uniform_align` apart.
  std::vector<uint8_t> uniforms(ops.size() * size_t(uniform_align), 0);
  for (size_t i = 0; i < ops.size(); i++) {
    std::memcpy(uniforms.data() + i * size_t(uniform_align), &ops[i].p, sizeof(Params));
  }
  wgpu::BufferDescriptor ud{};
  ud.label = "upscaler.params";
  ud.size = std::max<uint64_t>(uniforms.size(), 256);
  ud.usage = BU::Uniform | BU::CopyDst;
  const wgpu::Buffer params = device->device.CreateBuffer(&ud);
  device->queue.WriteBuffer(params, 0, uniforms.data(), uniforms.size());

  wgpu::CommandEncoder encoder = device->device.CreateCommandEncoder();
  wgpu::ComputePassEncoder pass;
  bool open = false;
  for (size_t i = 0; i < ops.size(); i++) {
    const Op& op = ops[i];
    if (op.copy) {
      if (open) pass.End();
      open = false;
      const uint64_t size = op.src.size ? op.src.size : op.src.buffer.GetSize() - op.src.offset;
      encoder.CopyBufferToBuffer(op.src.buffer, op.src.offset, op.dst.buffer, op.dst.offset, size);
      continue;
    }
    std::array<wgpu::BindGroupEntry, 6> e{};
    const Ref* refs[6] = {nullptr, &op.src, &op.wts, &op.bias, &op.dst, &op.aux};
    for (uint32_t b = 0; b < 6; b++) {
      e[b].binding = b;
      if (b == 0) {
        e[b].buffer = params;
        e[b].offset = i * uniform_align;
        e[b].size = sizeof(Params);
        continue;
      }
      const Ref& r = *refs[b];
      e[b].buffer = r.buffer ? r.buffer : dummy;
      e[b].offset = r.buffer ? r.offset : 0;
      e[b].size = r.buffer && r.size ? r.size : wgpu::kWholeSize;
    }
    wgpu::BindGroupDescriptor bd{};
    bd.layout = layout;
    bd.entryCount = e.size();
    bd.entries = e.data();
    const wgpu::BindGroup group = device->device.CreateBindGroup(&bd);
    if (!open) pass = encoder.BeginComputePass();
    open = true;
    pass.SetPipeline(pipelines.compute(pipeline[op.kernel]));
    pass.SetBindGroup(0, group);
    pass.DispatchWorkgroups(op.gx, op.gy, 1);
  }
  if (open) pass.End();
  wgpu::CommandBuffer commands = encoder.Finish();
  const uint32_t errors = device->errors.load();
  device->queue.Submit(1, &commands);
  ops.clear();
  if (device->errors.load() != errors || device->lost.load()) {
    error = "the upscaler's dispatches failed on " + gpu::adapter_line(*device) +
            " (the error lines above say why); run on the CPU";
    return false;
  }
  return true;
}

bool GpuUpscaler::run(const net::Map& rgb, net::Map& out, std::string& error) {
  using I = Impl;
  Impl& m = *impl_;
  const Upscaler& u = *m.net;
  const int h = rgb.h, w = rgb.w, c = u.embed;
  const uint32_t t = uint32_t(h) * uint32_t(w);
  const int hidden =
      u.groups.empty() || u.groups[0].blocks.empty() ? 0 : u.groups[0].blocks[0].fc1.out;
  const uint64_t big = uint64_t(t) * 256u * uint64_t(u.scale) * uint64_t(u.scale) / 4u;
  const uint64_t biggest =
      std::max<uint64_t>({big, uint64_t(t) * 3u * uint64_t(c), uint64_t(t) * 256u});
  if (biggest * 4 > m.device->limits.maxStorageBufferBindingSize ||
      biggest * 4 > m.device->limits.maxBufferSize) {
    error = "a " + std::to_string(w) + " x " + std::to_string(h) + " picture needs a " +
            std::to_string((biggest * 4) >> 20) + " MiB buffer, beyond " +
            gpu::adapter_line(*m.device) + "'s binding limit";
    return false;
  }
  // The maps (the upsampling's reuse the body's two largest).
  const wgpu::Buffer x0 = m.buffer(uint64_t(t) * 3, "upscaler.in");
  const wgpu::Buffer first = m.buffer(uint64_t(t) * c, "upscaler.first");
  const wgpu::Buffer body = m.buffer(uint64_t(t) * c, "upscaler.body");
  const wgpu::Buffer saved = m.buffer(uint64_t(t) * c, "upscaler.saved");
  const wgpu::Buffer normed = m.buffer(uint64_t(t) * c, "upscaler.normed");
  const wgpu::Buffer qkv = m.buffer(uint64_t(t) * 3 * c, "upscaler.qkv");
  const wgpu::Buffer conv = m.buffer(uint64_t(t) * c, "upscaler.conv");
  const wgpu::Buffer att = m.buffer(uint64_t(t) * c, "upscaler.att");
  const wgpu::Buffer mixed = m.buffer(uint64_t(t) * c, "upscaler.mixed");
  const wgpu::Buffer hid = m.buffer(uint64_t(t) * uint64_t(hidden), "upscaler.hidden");
  const wgpu::Buffer half1 = m.buffer(uint64_t(t) * uint64_t(hidden / 2), "upscaler.half1");
  const wgpu::Buffer half2 = m.buffer(uint64_t(t) * uint64_t(hidden / 2), "upscaler.half2");
  const wgpu::Buffer gated = m.buffer(uint64_t(t) * uint64_t(hidden / 2), "upscaler.gated");
  const wgpu::Buffer smid = m.buffer(uint64_t(t) * uint64_t(std::max(1, c / 16)), "upscaler.smid");
  const wgpu::Buffer sgate = m.buffer(t, "upscaler.sgate");
  // The small vectors: q's and k's sums of squares, the channel scores, a mean, the channel
  // gate's middle and its logits.
  const Ref norms = I::whole(m.buffer(2 * uint64_t(c), "upscaler.norms"));
  const Ref scores = I::whole(m.buffer(uint64_t(c) * uint64_t(c), "upscaler.scores"));
  const Ref mean = I::whole(m.buffer(uint64_t(c), "upscaler.mean"));
  const Ref cmid = I::whole(m.buffer(uint64_t(c), "upscaler.cmid"));
  const Ref cgate = I::whole(m.buffer(uint64_t(c), "upscaler.cgate"));
  const wgpu::Buffer big_a = m.buffer(big, "upscaler.big_a");
  const wgpu::Buffer big_b = m.buffer(big, "upscaler.big_b");

  std::vector<float> in(rgb.v);
  for (uint32_t i = 0; i < t; i++) {
    for (int ch = 0; ch < 3; ch++) in[size_t(i) * 3 + size_t(ch)] -= u.mean[size_t(ch)];
  }
  m.device->queue.WriteBuffer(x0, 0, in.data(), in.size() * 4);

  m.conv(m.first, I::whole(x0), I::whole(first), 0, h, w);
  m.layer_norm(m.before_w, m.before_b, I::whole(first), uint32_t(c), 0, I::whole(body), t,
               uint32_t(c));
  const uint32_t uc = uint32_t(c);
  for (size_t g = 0; g < u.groups.size(); g++) {
    m.copy(I::whole(body), I::whole(saved));
    for (size_t b = 0; b < u.groups[g].blocks.size(); b++) {
      const UpscalerBlock& k = u.groups[g].blocks[b];
      const BlockRefs& r = m.blocks[g][b];
      m.layer_norm(r.norm1_w, r.norm1_b, I::whole(body), uc, 0, I::whole(normed), t, uc);
      m.gemm(r.qkv, I::whole(normed), uc, 0, I::whole(qkv), 3 * uc, 0, t, 0);
      m.depthwise(r.dw_w, r.dw_b, I::whole(qkv), 3 * uc, 2 * uc, I::whole(conv), uc, h, w, true);
      if (k.spatial) {
        const int heads = u.heads / 2, hd = c / 2 / heads;
        const int longest = std::max(u.split[0], u.split[1]);
        const int ph = (h + longest - 1) / longest * longest,
                  pw = (w + longest - 1) / longest * longest;
        for (int half = 0; half < 2; half++) {
          const int hs = half == 0 ? u.split[0] : u.split[1];
          const int ws = half == 0 ? u.split[1] : u.split[0];
          Op op;
          op.kernel = k_window;
          op.p.rows = uint32_t(hs * ws);
          op.p.k = uint32_t(hd);
          op.p.n = uint32_t(heads);
          op.p.flags = k.shifted ? 1u : 0u;
          op.p.xs = 3 * uc;
          op.p.xo = uint32_t(half * c / 2);
          op.p.ys = uc;
          op.p.h = uint32_t(h);
          op.p.w = uint32_t(w);
          op.p.cin = uc;
          op.p.a0 = uint32_t(ph);
          op.p.a1 = uint32_t(pw);
          op.p.a2 = uint32_t(hs) | (uint32_t(ws) << 16);
          op.p.a3 = k.shifted ? uint32_t(hs / 2) | (uint32_t(ws / 2) << 16) : 0u;
          op.p.f0 = float(1.0 / std::sqrt(double(hd)));
          op.src = I::whole(qkv);
          op.wts = m.weight(r.bias[size_t(half)]);
          op.dst = I::whole(att);
          op.gx = uint32_t((ph / hs) * (pw / ws));
          op.gy = uint32_t(heads);
          m.ops.push_back(op);
        }
        // The channel gate off the convolution, the spatial gate off the attention.
        m.column_sum(I::whole(conv), uc, 0, t, uc, false, 1.0f / float(t), mean);
      } else {
        const int hd = c / u.heads;
        m.column_sum(I::whole(qkv), 3 * uc, 0, t, 2 * uc, true, 1.0f, norms);
        Op op;
        op.kernel = k_channel_scores;
        op.p.rows = t;
        op.p.k = uint32_t(hd);
        op.p.xs = 3 * uc;
        op.p.cin = uc;
        op.src = I::whole(qkv);
        op.wts = m.weight(r.temperature);
        op.aux = norms;
        op.dst = scores;
        op.gx = uc;
        op.gy = uint32_t(hd);
        m.ops.push_back(op);
        Params sp;
        sp.rows = uc;
        sp.k = uint32_t(hd);
        m.elementwise(k_softmax_rows, sp, {}, {}, {}, scores, {}, uc);
        Params ap;
        ap.rows = t;
        ap.n = uc;
        ap.k = uint32_t(hd);
        ap.xs = 3 * uc;
        ap.ys = uc;
        m.elementwise(k_channel_apply, ap, I::whole(qkv), {}, {}, I::whole(att), scores,
                      uint64_t(t) * uc);
        m.column_sum(I::whole(att), uc, 0, t, uc, false, 1.0f / float(t), mean);
      }
      // The channel gate's two 1 × 1 layers on the mean, then the spatial gate's on each token.
      m.gemm(r.channel1, mean, uc, 0, cmid, uint32_t(r.channel1.out), 0, 1, kGelu);
      m.gemm(r.channel2, cmid, uint32_t(r.channel1.out), 0, cgate, uc, 0, 1, 0);
      const Ref spatial_from = k.spatial ? I::whole(att) : I::whole(conv);
      m.gemm(r.spatial1, spatial_from, uc, 0, I::whole(smid), uint32_t(r.spatial1.out), 0, t,
             kGelu);
      m.gemm(r.spatial2, I::whole(smid), uint32_t(r.spatial1.out), 0, I::whole(sgate), 1, 0, t, 0);
      Params gp;
      gp.rows = t;
      gp.n = uc;
      gp.flags = k.spatial ? 1u : 0u;
      m.elementwise(k_gate_mix, gp, I::whole(att), cgate, I::whole(sgate), I::whole(mixed),
                    I::whole(conv), uint64_t(t) * uc);
      m.gemm(r.proj, I::whole(mixed), uc, 0, I::whole(body), uc, 0, t, kAdd);
      // The feed-forward.
      const uint32_t hh = uint32_t(hidden / 2);
      m.layer_norm(r.norm2_w, r.norm2_b, I::whole(body), uc, 0, I::whole(normed), t, uc);
      m.gemm(r.fc1, I::whole(normed), uc, 0, I::whole(hid), uint32_t(hidden), 0, t, kGelu);
      m.layer_norm(r.gate_norm_w, r.gate_norm_b, I::whole(hid), uint32_t(hidden), hh,
                   I::whole(half1), t, hh);
      m.depthwise(r.gate_w, r.gate_b, I::whole(half1), hh, 0, I::whole(half2), hh, h, w, false);
      Params mp;
      mp.rows = t;
      mp.n = hh;
      mp.xs = uint32_t(hidden);
      m.elementwise(k_multiply, mp, I::whole(hid), {}, {}, I::whole(gated), I::whole(half2),
                    uint64_t(t) * hh);
      m.gemm(r.fc2, I::whole(gated), hh, 0, I::whole(body), uc, 0, t, kAdd);
    }
    // The group's convolution added to what came in, then that is the body.
    m.conv(m.group_conv[g], I::whole(body), I::whole(saved), kAdd, h, w);
    m.copy(I::whole(saved), I::whole(body));
  }
  m.layer_norm(m.norm_w, m.norm_b, I::whole(body), uc, 0, I::whole(normed), t, uc);
  m.conv(m.after_body, I::whole(normed), I::whole(first), kAdd, h, w);
  const wgpu::Buffer features =
      m.buffer(uint64_t(t) * uint64_t(m.before_up.out), "upscaler.features");
  m.conv(m.before_up, I::whole(first), I::whole(features), kLeaky, h, w);
  // Upsampling: the convolution into big_a, the shuffle into big_b, which is the next input.
  Ref from = I::whole(features);
  int uh = h, uw = w;
  for (const LinearRef& l : m.up) {
    m.conv(l, from, I::whole(big_a), 0, uh, uw);
    Params sp;
    sp.h = uint32_t(uh);
    sp.w = uint32_t(uw);
    sp.n = uint32_t(l.out / 4);
    m.elementwise(k_shuffle, sp, I::whole(big_a), {}, {}, I::whole(big_b), {},
                  uint64_t(uh) * uint64_t(uw) * uint64_t(l.out));
    from = I::whole(big_b);
    uh *= 2;
    uw *= 2;
  }
  const wgpu::Buffer result = m.buffer(uint64_t(uh) * uint64_t(uw) * 3, "upscaler.out");
  m.conv(m.last, from, I::whole(result), 0, uh, uw);
  if (!m.submit(error)) return false;

  const uint64_t bytes = uint64_t(uh) * uint64_t(uw) * 3 * 4;
  wgpu::BufferDescriptor rd{};
  rd.label = "upscaler.readback";
  rd.size = bytes;
  rd.usage = BU::MapRead | BU::CopyDst;
  const wgpu::Buffer rb = m.device->device.CreateBuffer(&rd);
  wgpu::CommandEncoder encoder = m.device->device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(result, 0, rb, 0, bytes);
  wgpu::CommandBuffer commands = encoder.Finish();
  m.device->queue.Submit(1, &commands);
  gpu::wait(*m.device, rb.MapAsync(wgpu::MapMode::Read, 0, bytes, wgpu::CallbackMode::WaitAnyOnly,
                                   [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  out = net::Map(uh, uw, 3);
  const void* p = rb.GetConstMappedRange(0, bytes);
  if (p == nullptr || m.device->lost.load()) {
    error = "the upscaler's result could not be read back from " + gpu::adapter_line(*m.device);
    return false;
  }
  std::memcpy(out.v.data(), p, bytes);
  rb.Unmap();
  for (size_t i = 0; i < out.tokens(); i++) {
    for (int ch = 0; ch < 3; ch++) out.v[i * 3 + size_t(ch)] += u.mean[size_t(ch)];
  }
  return true;
}

}  // namespace ge::classicenhance
