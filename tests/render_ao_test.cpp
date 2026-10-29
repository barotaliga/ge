// engine/render/ao.hpp: the target is half the render size, rounded up; on a device, a wall
// square to the view reads open, a valley running up the picture reads closed along its floor
// and more than its walls, a ridge pointing at the eye reads open, and where nothing was drawn
// reads open with no depth; each texel carries the depth it was worked at.
//
// ge:summary Tests for ambient occlusion: its size, a flat wall, a valley, a ridge and the sky
// ge:keywords test render ambient-occlusion ao gtao valley ridge half-size
// ge:skill render testing
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu_support.hpp"
#include "render/ao.hpp"
#include "render/frame.hpp"
#include "render/view.hpp"

namespace {

namespace r = ge::render;

constexpr uint32_t kW = 96, kH = 64;

// A full-screen triangle writing only a depth: at `kNear` (reversed-z: 0.01 is 100 near planes,
// 10 m) times or over (1 + kSlope x the pixels from the middle column), so the surface's distance
// shrinks or grows away from the middle; kSlope 0 is a wall square to the view, `kValley` 1 a
// valley whose floor is the middle column (the sides nearer) and 0 a ridge pointing at the eye.
constexpr const char* kShapes = R"(
override kNear: f32 = 0.01;
override kSlope: f32 = 0.0;
override kValley: f32 = 1.0;
override kDepth: f32 = 0.01;

@vertex
fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
  return vec4f(p * 2.0 - 1.0, 0.5, 1.0);
}

@fragment
fn fs(@builtin(position) at: vec4f) -> @builtin(frag_depth) f32 {
  let a = 1.0 + kSlope * abs(at.x - 48.0);
  return select(kNear / a, kNear * a, kValley > 0.5) * select(1.0, 0.0, kDepth == 0.0);
}
)";

struct Scene {
  ge::gpu::PipelineSet pipelines;
  wgpu::Buffer uniforms;
  r::FrameUniforms staged{};
  r::Ao ao;
  r::FrameTargets targets;
  r::FrameRecorder rec;
  uint32_t flat = 0, valley = 0, ridge = 0, sky = 0;
};

uint32_t shape(ge::gpu::Device& d, Scene& s, const wgpu::ShaderModule& m, double slope,
               double valley, double depth) {
  wgpu::ConstantEntry c[3] = {};
  c[0].key = "kSlope";
  c[0].value = slope;
  c[1].key = "kValley";
  c[1].value = valley;
  c[2].key = "kDepth";
  c[2].value = depth;
  wgpu::DepthStencilState z{};
  z.format = r::kDepthFormat;
  z.depthWriteEnabled = true;
  z.depthCompare = wgpu::CompareFunction::Always;
  wgpu::FragmentState fragment{};
  fragment.module = m;
  fragment.entryPoint = "fs";
  fragment.constantCount = 3;
  fragment.constants = c;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "test.shape";
  desc.vertex.module = m;
  desc.vertex.entryPoint = "vs";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &z;
  desc.fragment = &fragment;
  return s.pipelines.add_render(d, desc);
}

void make_scene(ge::gpu::Device& d, Scene& s) {
  s.uniforms =
      r::make_buffer(d, "test.uniforms", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
  r::create_ao_pipelines(d, s.pipelines, s.ao);
  const wgpu::ShaderModule m = ge::gpu::create_shader(d, kShapes, "test.shapes");
  s.flat = shape(d, s, m, 0, 1, 1);
  s.valley = shape(d, s, m, 0.03, 1, 1);
  s.ridge = shape(d, s, m, 0.03, 0, 1);
  s.sky = shape(d, s, m, 0, 1, 0);
  CHECK(s.pipelines.finish(d));
  ge::gpu::end_load(d);
  s.targets = r::create_targets(d, "test", kW, kH);
  r::create_ao_targets(d, s.ao, kW, kH, s.uniforms, s.targets.depth_view);
  r::set_ao(d, s.ao, {.strength = 1, .radius_m = 1.5});
  s.rec.init(d, "test.timer", 2);
}

// The shape drawn into the depth, the pass run; the target's red (the occlusion) and green (the
// depth it was worked at) back.
void run(ge::gpu::Device& d, Scene& s, uint32_t pipeline, std::vector<float>& shut,
         std::vector<float>& depth) {
  r::FrameView v;
  v.altitude_m = 1000;
  r::fill_uniforms(s.staged, v, kW, kH, nullptr);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  s.rec.begin_frame(d);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  {
    wgpu::RenderPassDepthStencilAttachment z{};
    z.view = s.targets.depth_view;
    z.depthLoadOp = wgpu::LoadOp::Clear;
    z.depthStoreOp = wgpu::StoreOp::Store;
    z.depthClearValue = 0;
    wgpu::RenderPassDescriptor desc{};
    desc.depthStencilAttachment = &z;
    wgpu::RenderPassEncoder pass = s.rec.render(e, desc, "shape");
    pass.SetPipeline(s.pipelines.render(pipeline));
    pass.Draw(3);
    pass.End();
  }
  r::encode_ao(s.ao, s.pipelines, s.rec, e);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  shut = r::read_red(d, s.ao.target, 0, s.ao.width, s.ao.height, r::kHdrFormat);
  depth = r::read_red(d, s.ao.target, 0, s.ao.width, s.ao.height, r::kHdrFormat, 1);
}

// The texel at half-size column `x` on the middle row.
float at(const std::vector<float>& v, const Scene& s, uint32_t x) {
  return v[static_cast<size_t>(s.ao.height / 2) * s.ao.width + x];
}

}  // namespace

TEST(the_target_is_half_the_render_size_rounded_up) {
  uint32_t w, h;
  r::ao_size(1280, 720, &w, &h);
  CHECK(w == 640 && h == 360);
  r::ao_size(1281, 1, &w, &h);
  CHECK(w == 641 && h == 1);
  CHECK(!r::ao_on({}) && r::ao_on({.strength = 0.1}));
}

TEST(a_wall_and_a_ridge_read_open_and_a_valley_reads_closed_along_its_floor) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::Device& d = *device;
  Scene s;
  make_scene(d, s);
  std::vector<float> shut, depth;
  run(d, s, s.flat, shut, depth);
  CHECK(at(shut, s, 24) == 0.0f && at(shut, s, 8) == 0.0f);
  CHECK(at(depth, s, 24) > 0.009f && at(depth, s, 24) < 0.011f);
  run(d, s, s.valley, shut, depth);
  const float floor = at(shut, s, 24), wall = at(shut, s, 8);
  CHECK(floor > 0.1f);
  CHECK(floor > wall);
  run(d, s, s.ridge, shut, depth);
  CHECK(at(shut, s, 24) == 0.0f);
  run(d, s, s.sky, shut, depth);
  for (size_t i = 0; i < shut.size(); ++i) CHECK(shut[i] == 0.0f && depth[i] == 0.0f);
  CHECK(d.errors.load() == 0);
}
