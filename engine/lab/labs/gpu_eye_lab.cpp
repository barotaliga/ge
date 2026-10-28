// LAB `gpu-eye`: the eye's adaptation (engine/render/eye.hpp) walking out of a dark hall into a
// snowy noon and stepping back into the dark. The scene is the engine's own terrain and sky with
// a 40 m stone corridor drawn over them from inside (gpu_eye_hall.wgsl: its walls lit by the day
// through its door and by one lamp, tens to hundreds of times darker than outside). Two walks:
//
//   OUT    adapted deep inside, then out through the door at a jog and on into the snow: the
//          hall opens up, the door blazes, the day dazzles for a moment past the door, then
//          settles to the sky's own exposure
//   IN     adapted to the day outside, then stood deep in the hall facing its dead end: black
//          at first, the walls coming up out of the dark over seconds
//
// Each moment is pictured twice, exposed by the sky alone (the eye at rest, as before this
// pass) and by the adapted eye, side by side, and the eye's stops and its goal are charted
// against the clock with the metered average. GPU pictures and timings are measured, not hashed.
//
// ge:summary Lab: the eye adapting out of a dark hall into daylight and back into the dark
// ge:keywords lab gpu eye eye-adaptation auto-exposure hall corridor dazzle dark-adaptation walk
// ge:skill render labs
// ge:test tests/render_eye_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/gpu_eye_hall.wgsl.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/raster.hpp"
#include "render/eye.hpp"
#include "render/frame.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace libm = ge::core::libm;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::paste;
using ge::lab::water_scene::placeholders;
using ge::lab::water_scene::view_at;
using gl::Det;
constexpr double kPi = 3.14159265358979323846;

// The corridor (world, m): its inside, its door in the z = kHallZ1 wall.
constexpr float kHallLo[3] = {-2, 0, -40}, kHallHi[3] = {2, 3.5f, 0};

struct HallBlock {
  float lo[4], hi[4], door[4], lamp[4];
};

struct Scene {
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms, hall;
  gr::FrameUniforms staged{};
  wgpu::BindGroupLayout layout, hall_layout;
  wgpu::BindGroup frame, post_group, eye_group, hall_group;
  gr::Sky sky;
  gr::Terrain terrain;
  gr::Post post;
  gr::Eye eye;
  gr::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
  ge::gpu::Target out;
  uint32_t p_hall = 0;
};

// Flat snow round the hall (world x, z), hills rising from 90 m off the point 20 m out of its door.
float height_at(double x, double z) {
  const double r = std::sqrt(x * x + (z - 20) * (z - 20));
  const double rise = r > 90 ? (r - 90) * 0.45 : 0;
  return static_cast<float>(rise + (r > 60 ? 2 * libm::sin(x * 0.05) * libm::sin(z * 0.04) : 0));
}

bool make_scene(ge::gpu::Device& d, Scene& s, uint32_t w, uint32_t h) {
  constexpr int n = 257;
  s.field = ge::core::create_heightfield(-128, -88, 1.0, n, n);
  s.albedo.resize(size_t{n} * n * 4);
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      const size_t i = static_cast<size_t>(r) * n + static_cast<size_t>(c);
      s.field.data[i] = height_at(c - 128.0, r - 88.0);
      const uint8_t px[4] = {236, 240, 246, 255};
      std::memcpy(&s.albedo[i * 4], px, 4);
    }
  }
  s.targets = gr::create_targets(d, "gpu-eye", w, h);
  s.uniforms =
      gr::make_buffer(d, "gpu-eye.uniforms", sizeof(gr::FrameUniforms), wgpu::BufferUsage::Uniform);
  s.layout = gr::frame_layout(d);
  gr::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  gr::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.base_spacing = 1.0;
  src.levels = 7;
  gr::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  gr::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  gr::create_eye(d, s.pipelines, s.eye, s.layout, s.sky);
  s.hall_layout =
      gr::make_layout(d, "gpu-eye.hall", {gr::Slot::uniform}, wgpu::ShaderStage::Fragment);
  const wgpu::ShaderModule module = ge::gpu::create_shader(d, ge::wgsl::gpu_eye_hall, "hall");
  wgpu::ColorTargetState colour{};
  colour.format = gr::kHdrFormat;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 1;
  fragment.targets = &colour;
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "gpu-eye.hall";
  desc.layout = gr::make_pipeline_layout(d, "gpu-eye.hall", {s.layout, s.hall_layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.fragment = &fragment;
  s.p_hall = s.pipelines.add_render(d, desc);
  if (!s.pipelines.finish(d)) return false;
  ge::gpu::end_load(d);
  s.frame = gr::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = gr::post_group(d, s.post, s.targets.hdr_view);
  s.eye_group = gr::eye_group(d, s.eye, s.targets.hdr_view);
  s.hall = gr::make_buffer(d, "gpu-eye.hall", sizeof(HallBlock),
                           wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
  const HallBlock hb{{kHallLo[0], kHallLo[1], kHallLo[2], 0},
                     {kHallHi[0], kHallHi[1], kHallHi[2], 0},
                     {1.2f, 2.6f, 0.35f, 0.7f},
                     {-1.7f, 2.5f, -24, 0.12f}};
  d.queue.WriteBuffer(s.hall, 0, &hb, sizeof hb);
  s.hall_group = gr::make_group(d, "gpu-eye.hall", s.hall_layout, {s.hall});
  s.rec.init(d, "gpu-eye.timer", 10);
  s.out = ge::gpu::create_target(d, "gpu-eye.final", w, h);
  return true;
}

// One frame `dt` s after the last at `v`; `adapt` runs the eye. The picture when `read`.
gl::Picture frame_at(ge::gpu::Device& d, Scene& s, const gr::FrameView& v, bool adapt, double dt,
                     bool read) {
  s.rec.begin_frame(d);
  gr::fill_uniforms(s.staged, v, s.targets.width, s.targets.height);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  gr::prepare_sky(s.sky, v);
  gr::prepare_terrain(d, s.terrain, v);
  if (adapt) gr::prepare_eye(d, s.eye, dt);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  gr::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  gr::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  gr::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  {
    wgpu::RenderPassColorAttachment colour{};
    colour.view = s.targets.hdr_view;
    colour.loadOp = wgpu::LoadOp::Load;
    colour.storeOp = wgpu::StoreOp::Store;
    wgpu::RenderPassDescriptor desc{};
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &colour;
    wgpu::RenderPassEncoder pass = s.rec.render(e, desc, "hall");
    pass.SetPipeline(s.pipelines.render(s.p_hall));
    pass.SetBindGroup(0, s.frame);
    pass.SetBindGroup(1, s.hall_group);
    pass.Draw(3);
    pass.End();
  }
  if (adapt) gr::encode_eye(s.eye, s.pipelines, s.rec, e, s.frame, s.eye_group);
  gr::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, s.out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  if (!read) return {};
  const ge::gpu::Image img = ge::gpu::read_now(d, s.out);
  return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
}

constexpr double kSunEl = 0.62, kSunAz = 2.6;  // a winter noon, the sun off the right shoulder

// THE OUT WALK: held 2 s at z -34, then 4 m/s along the corridor and 6 m past the door, held.
double out_z(double t) { return t < 2 ? -34 : std::fmin(-34 + 4 * (t - 2), 6.0); }
gr::FrameView out_view(double t) { return view_at(0, 1.7, out_z(t), 0, -0.04, kSunEl, kSunAz, t); }
// THE IN WALK: outside looking out for 1 s, then stood at z -20 looking at the dead end.
gr::FrameView in_view(double t) {
  if (t < 1) return view_at(0, 1.7, 6, 0, -0.04, kSunEl, kSunAz, t);
  return view_at(0, 1.7, -20, kPi, -0.04, kSunEl, kSunAz, t);
}

struct Moment {
  double t;
  const char* label;
};

void walk(gl::Lab& lab, ge::gpu::Device& d, Scene& s, const char* id, const char* title,
          gr::FrameView (*view)(double), double seconds, const std::vector<Moment>& moments,
          double fps, const std::string& caption, const std::string& look_for) {
  const int w = static_cast<int>(s.targets.width), h = static_cast<int>(s.targets.height);
  // The sky alone at each moment (the eye never runs, so it stays at rest).
  std::vector<gl::Picture> rest;
  rest.reserve(moments.size());
  for (const Moment& m : moments) rest.push_back(frame_at(d, s, view(m.t), false, 0, true));
  // The eye, frame by frame.
  gl::Trace open{"eye opened (stops)", {}, {}}, goal{"its goal (stops)", {}, {}},
      avg{"metered average + 4 (log2)", {}, {}};
  std::vector<gl::Picture> eyed(moments.size());
  const double dt = 1 / fps;
  gr::cut_eye(s.eye);
  size_t next = 0;
  const int frames = static_cast<int>(std::lround(seconds * fps));
  for (int f = 0; f <= frames; ++f) {
    const double t = f * dt;
    const bool shoot = next < moments.size() && t + 0.5 * dt >= moments[next].t;
    gl::Picture p = frame_at(d, s, view(t), true, dt, shoot);
    const gr::EyeReading r = gr::read_eye(d, s.sky);
    open.x.push_back(t);
    open.y.push_back(r.open_ev);
    goal.x.push_back(t);
    goal.y.push_back(r.target_ev);
    avg.x.push_back(t);
    avg.y.push_back(r.average + 4);
    if (shoot) eyed[next++] = std::move(p);
  }
  // The sheet: a row a moment, the sky's exposure left and the eye's right.
  const int band = 18;
  gl::Picture sheet = gl::create_picture(
      2 * w + 12, static_cast<int>(moments.size()) * (h + band) + band, {24, 24, 28});
  gl::draw_text(sheet, 4, 4, "SKY'S EXPOSURE (THE EYE AT REST)", {230, 230, 230});
  gl::draw_text(sheet, w + 16, 4, "THE ADAPTED EYE", {230, 230, 230});
  for (size_t k = 0; k < moments.size(); ++k) {
    const int y = band + static_cast<int>(k) * (h + band);
    paste(sheet, rest[k], 4, y);
    if (eyed[k].width > 0) paste(sheet, eyed[k], w + 8, y);
    const std::string label = fixed(moments[k].t, 1) + " S: " + moments[k].label;
    gl::draw_text(sheet, 8, y + h + 4, label, {230, 230, 230});
    const size_t at = static_cast<size_t>(std::lround(moments[k].t * fps));
    if (at < open.y.size()) {
      gl::draw_text(sheet, w + 12, y + h + 4, "OPEN " + fixed(open.y[at], 2) + " STOPS",
                    {230, 230, 230});
    }
  }
  lab.shot({std::string(id) + "-sheet",
            title,
            caption,
            look_for,
            {{"size", std::to_string(w) + "x" + std::to_string(h)}, {"fps", fixed(fps, 0)}}},
           sheet, Det::measured);
  gl::LineChart line;
  line.frame = {std::string(title) + ": THE EYE AGAINST THE CLOCK",
                "the eye's stops past the sky's exposure, its goal, and the metered average",
                "t (s)", "stops"};
  line.traces = {open, goal, avg};
  line.spikes = false;
  for (const Moment& m : moments) line.marks.push_back({m.t, m.label});
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({std::string(id) + "-chart",
            chart.title,
            "Every frame's reading of the eye: how many stops it has opened past the sky's "
            "exposure, the goal it steps toward, and the meter's average (log2 at the sky's "
            "exposure, lifted 4 to share the axis).",
            "the goal jumps where the light changes; the eye follows it fast toward the light "
            "(a third of a second a half) and slowly toward the dark (2.5 s a half)",
            {{"fps", fixed(fps, 0)}}},
           gl::to_picture(chart.plot), Det::measured);
  lab.metric(std::string(id) + ".most-open", *std::max_element(open.y.begin(), open.y.end()),
             "stops", Det::measured);
  lab.metric(std::string(id) + ".last-open", open.y.back(), "stops", Det::measured);
}

void run(gl::Lab& lab) {
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 9 / 16;
  const double fps = lab.arg_num("fps");
  const std::vector<Moment> out_moments = {{1.5, "deep inside"},   {6.5, "halfway out"},
                                           {9.5, "near the door"}, {10.7, "just out"},
                                           {11.5, "out 1 s"},      {13.5, "out 3 s"}};
  const std::vector<Moment> in_moments = {
      {0.9, "outside"}, {1.2, "just in"}, {3.0, "in 2 s"}, {6.0, "in 5 s"}, {10.0, "in 9 s"}};
  auto device = ge::gpu::create_device({});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU adapter (GE_GPU_REQUIRE is set)",
               "next: install a Vulkan driver (lavapipe) or unset GE_GPU_REQUIRE; skill: gpu");
      return;
    }
    placeholders(lab, {"out-sheet", "out-chart", "in-sheet", "in-chart"}, w, h);
    return;
  }
  ge::gpu::Device& d = *device;
  Scene s;
  if (!make_scene(d, s, w, h)) {
    lab.fail("the eye lab's pipelines failed", "Read the error lines above; skill: render.");
    return;
  }
  walk(lab, d, s, "out", "OUT OF THE HALL INTO THE DAY", out_view, 14, out_moments, fps,
       "A 40 m stone corridor lit by its door and one lamp, walked out of at 4 m/s into the "
       "noon snow after 2 s held deep inside. Left: exposed by the sky alone, as before the eye "
       "existed: the hall black but for the door and the lamp. Right: the adapted eye.",
       "inside, the walls and the floor readable on the right and black on the left; just out, "
       "the right column over-bright (the dazzle) and back to the left's look within a second");
  walk(lab, d, s, "in", "INTO THE DARK", in_view, 10, in_moments, fps,
       "Adapted to the noon snow outside, then stood in the corridor 20 m in, facing its dead "
       "end. Left: the sky's exposure. Right: the eye opening to the dark over seconds.",
       "the right column black at first and the walls coming up out of the dark over the next "
       "seconds, never past the sky's exposure plus max_ev (4 stops)");
  // The same two metrics the placeholders give, so a machine with no adapter digests alike.
  const int errors = static_cast<int>(d.errors.load());
  const int late = static_cast<int>(d.pipelines_after_load);
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0 || late != 0)
    lab.fail("the device reported " + std::to_string(errors) + " errors and " +
                 std::to_string(late) + " pipelines built after the load",
             "next: read the validation lines above; run make shaders-check; skill: gpu");
}

const gl::LabSpec kSpec{
    "gpu-eye",
    gl::LabKind::picture,
    "engine",
    "engine/render/eye.hpp",
    "render",
    "Does the eye open up in a dark hall, dazzle for a moment walking out into the day, and open "
    "slowly again back in the dark?",
    {{"width", "640", "a view's width, px (16:9)"}, {"fps", "20", "frames a second the walks run"}},
    "--width=256 --fps=10",
    run};
GE_LAB(kSpec);

}  // namespace
