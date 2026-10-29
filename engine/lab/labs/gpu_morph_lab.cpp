// LAB `gpu-morph`: MORPH MESHES (render/morph.hpp) on the software adapter: a runner built of
// boxes in code, animated by eight whole frames of a run (its limbs swung about the hips and
// shoulders, its body bobbing), its skin painted with two tinted regions (the shirt and the
// arms in region A, the trousers in B, each a ramp of steps so the shading is the skin's) and
// glowing eyes. Named views: four runners in four teams' colours on a yard by day; one runner's
// skin read sharp (the classic look) beside smooth; the run's frames and the frames drawn
// between them at quarter steps; the team at dusk with their eyes glowing; and a runner lit
// indoors by a game's own light (no sun or sky, an ambient and a shade from one side). Checks:
// each team's shirt reads as its colour, the eyes glow at dusk, the in-between frame lies
// between its two frames, no GPU error, no pipeline after the load. Measured: the pass's GPU
// time for 64 runners. Without an adapter it notes that, with grey cards.
//
// ge:summary Lab: morph meshes in team colours, sharp and smooth skins, frame blending, glow
// ge:keywords lab gpu morph morph-mesh vertex-animation frames blend tint team-colours
// ge:keywords player-colours skin sharp smooth glow indoor-light runner picture
// ge:skill render labs
// ge:test tests/render_morph_test.cpp
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/raster.hpp"
#include "render/frame.hpp"
#include "render/morph.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace libm = ge::core::libm;
namespace es = ge::lab::explosion_scene;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::placeholders;
using gl::Det;

// ── The runner, built in code ──

constexpr uint32_t kSkin = 64;
constexpr uint32_t kFrames = 8;

struct Rect {
  uint32_t x0, y0, x1, y1;
};
// The skin's regions.
constexpr Rect kShirt{0, 0, 24, 24}, kTrousers{24, 0, 40, 24}, kSleeve{40, 0, 52, 24},
    kFace{0, 24, 16, 40}, kHair{16, 24, 32, 40}, kBoot{32, 24, 40, 32};

struct Part {
  float centre[3];
  float half[3];
  float pivot[3];
  int swing;  // 0: none, +1 or -1: swung with the legs' phase, 2 or -2: with the arms'
  Rect front, other;
};

const Part kParts[] = {
    {{0, 1.15f, 0}, {0.22f, 0.3f, 0.12f}, {0, 0, 0}, 0, kShirt, kShirt},
    {{0, 1.62f, 0}, {0.12f, 0.14f, 0.12f}, {0, 0, 0}, 0, kFace, kHair},
    {{-0.11f, 0.47f, 0}, {0.09f, 0.38f, 0.09f}, {-0.11f, 0.85f, 0}, 1, kTrousers, kTrousers},
    {{0.11f, 0.47f, 0}, {0.09f, 0.38f, 0.09f}, {0.11f, 0.85f, 0}, -1, kTrousers, kTrousers},
    {{-0.11f, 0.05f, 0.04f}, {0.09f, 0.05f, 0.13f}, {-0.11f, 0.85f, 0}, 1, kBoot, kBoot},
    {{0.11f, 0.05f, 0.04f}, {0.09f, 0.05f, 0.13f}, {0.11f, 0.85f, 0}, -1, kBoot, kBoot},
    {{-0.3f, 1.13f, 0}, {0.07f, 0.29f, 0.07f}, {-0.3f, 1.4f, 0}, -2, kSleeve, kSleeve},
    {{0.3f, 1.13f, 0}, {0.07f, 0.29f, 0.07f}, {0.3f, 1.4f, 0}, 2, kSleeve, kSleeve},
};

// A face of a box: its normal and two axes with u x v = n.
struct Face {
  float n[3], u[3], v[3];
};
const Face kFaces[] = {
    {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
    {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
    {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}},  {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
};

// x rotated by `a` (rad) about the x axis round `pivot`.
void swing(float p[3], const float pivot[3], double a) {
  const double c = libm::cos(a), s = libm::sin(a);
  const double y = p[1] - pivot[1], z = p[2] - pivot[2];
  p[1] = static_cast<float>(pivot[1] + c * y - s * z);
  p[2] = static_cast<float>(pivot[2] + s * y + c * z);
}

gr::MorphModel runner() {
  gr::MorphModel m;
  m.positions.resize(kFrames);
  m.normals.resize(kFrames);
  for (const Part& part : kParts) {
    for (const Face& f : kFaces) {
      const uint32_t base = static_cast<uint32_t>(m.uv.size() / 2);
      const Rect r = f.n[2] > 0 ? part.front : part.other;
      const float corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
      for (const auto& c : corners) {
        m.uv.push_back(static_cast<float>(c[0] < 0 ? r.x0 : r.x1) / static_cast<float>(kSkin));
        m.uv.push_back(static_cast<float>(c[1] < 0 ? r.y1 : r.y0) / static_cast<float>(kSkin));
        for (uint32_t k = 0; k < kFrames; ++k) {
          const double phase = 6.283185307179586 * k / kFrames;
          float p[3], n[3];
          for (int i = 0; i < 3; ++i) {
            p[i] = part.centre[i] + part.half[i] * (f.n[i] + c[0] * f.u[i] + c[1] * f.v[i]);
            n[i] = f.n[i];
          }
          if (part.swing != 0) {
            const double amp = part.swing == 1 || part.swing == -1 ? 0.6 : 0.5;
            const double sign = part.swing > 0 ? 1 : -1;
            const double a = sign * amp * libm::sin(phase);
            swing(p, part.pivot, a);
            const float zero[3] = {0, 0, 0};
            swing(n, zero, a);
          }
          p[1] += static_cast<float>(0.05 * std::abs(libm::sin(phase)));
          m.positions[k].insert(m.positions[k].end(), p, p + 3);
          m.normals[k].insert(m.normals[k].end(), n, n + 3);
        }
      }
      m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
  }
  // The skin: the shirt shaded down its height with a light band across the chest, the trousers
  // and the sleeves shaded likewise, the face plain with two glowing eyes, hair and boots.
  m.skin.width = m.skin.height = kSkin;
  m.skin.rgba.assign(size_t{kSkin} * kSkin * 4, 0);
  m.skin.marks.assign(size_t{kSkin} * kSkin, gr::kMarkPlain);
  auto fill = [&](Rect r, uint8_t red, uint8_t green, uint8_t blue) {
    for (uint32_t y = r.y0; y < r.y1; ++y) {
      for (uint32_t x = r.x0; x < r.x1; ++x) {
        uint8_t* t = &m.skin.rgba[(size_t{y} * kSkin + x) * 4];
        t[0] = red;
        t[1] = green;
        t[2] = blue;
        t[3] = 255;
      }
    }
  };
  auto tint = [&](Rect r, uint8_t region, int top, int bottom, bool band) {
    for (uint32_t y = r.y0; y < r.y1; ++y) {
      for (uint32_t x = r.x0; x < r.x1; ++x) {
        int step =
            top + (bottom - top) * static_cast<int>(y - r.y0) / static_cast<int>(r.y1 - r.y0);
        if (band && y >= r.y0 + 8 && y < r.y0 + 11) step = 1;
        if (x == r.x0 || x + 1 == r.x1) step = step + 3 > 15 ? 15 : step + 3;  // the seams
        m.skin.marks[size_t{y} * kSkin + x] = static_cast<uint8_t>(region | step);
      }
    }
  };
  tint(kShirt, gr::kMarkTintA, 4, 11, true);
  tint(kTrousers, gr::kMarkTintB, 5, 13, false);
  tint(kSleeve, gr::kMarkTintA, 5, 10, false);
  fill(kFace, 214, 168, 136);
  fill(kHair, 74, 52, 36);
  fill(kBoot, 40, 38, 36);
  for (const uint32_t x : {4u, 5u, 10u, 11u}) {
    for (const uint32_t y : {30u, 31u}) {
      fill({x, y, x + 1, y + 1}, 255, 236, 140);
      m.skin.marks[size_t{y} * kSkin + x] = gr::kMarkGlow;
    }
  }
  return m;
}

// The teams: shirt and trousers.
struct Team {
  const char* name;
  float shirt[3], trousers[3];
};
const Team kTeams[] = {
    {"red", {0.62f, 0.05f, 0.04f}, {0.2f, 0.06f, 0.05f}},
    {"blue", {0.05f, 0.12f, 0.62f}, {0.08f, 0.08f, 0.2f}},
    {"green", {0.08f, 0.45f, 0.06f}, {0.32f, 0.26f, 0.12f}},
    {"gold", {0.7f, 0.45f, 0.03f}, {0.05f, 0.05f, 0.05f}},
};

// ── The scene ──

struct Scene {
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms;
  gr::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group;
  gr::Sky sky;
  gr::Terrain terrain;
  gr::Morph morph;
  gr::Post post;
  gr::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

bool make_scene(ge::gpu::Device& d, Scene& s, uint32_t w, uint32_t h,
                const std::vector<gr::MorphModel>& models, const gr::TintRamps& ramps) {
  s.w = w;
  s.h = h;
  s.field = ge::core::create_heightfield(-64, -64, 1, 129, 129);
  s.albedo.resize(size_t{129} * 129 * 4);
  for (int r = 0; r < 129; ++r) {
    for (int c = 0; c < 129; ++c) {
      const size_t i = static_cast<size_t>(r) * 129 + static_cast<size_t>(c);
      const int m = ((c / 2 + r / 2) & 1) * 10 + ((c * 7 + r * 13) % 7) - 3;
      const uint8_t px[4] = {static_cast<uint8_t>(126 + m), static_cast<uint8_t>(118 + m),
                             static_cast<uint8_t>(104 + m), 255};
      std::memcpy(&s.albedo[i * 4], px, 4);
    }
  }
  s.targets = gr::create_targets(d, "gpu-morph", w, h);
  s.uniforms = gr::make_buffer(d, "gpu-morph.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  s.layout = gr::frame_layout(d);
  gr::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  gr::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.base_spacing = 1.0;
  src.levels = 6;
  gr::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  if (!gr::create_morph(d, s.pipelines, s.morph, s.layout, models, ramps)) return false;
  gr::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  if (!s.pipelines.finish(d)) return false;
  ge::gpu::end_load(d);
  s.frame = gr::frame_group(d, s.layout, s.uniforms, s.sky);
  s.post_group = gr::post_group(d, s.post, s.targets.hdr_view);
  s.rec.init(d, "gpu-morph.timer", 8);
  s.out = ge::gpu::create_target(d, "gpu-morph.final", w, h);
  return true;
}

struct Timed {
  gl::Picture pic;
  double ms = 0;  // the morph pass on the GPU, NaN when not measured
};

Timed draw(ge::gpu::Device& d, Scene& s, const gr::FrameView& v,
           const std::vector<gr::MorphDraw>& draws) {
  s.rec.begin_frame(d);
  gr::fill_uniforms(s.staged, v, s.w, s.h);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  gr::prepare_sky(s.sky, v);
  gr::prepare_terrain(d, s.terrain, v);
  gr::prepare_morph(d, s.morph, draws, v);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  gr::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  gr::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  gr::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_morph(s.morph, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, s.out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, s.out);
  s.rec.wait(d);
  Timed out{gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
            0.0 / 0.0};
  if (s.rec.timed) {
    for (int i = 0; i < s.rec.stats.count; ++i) {
      const gr::PassStat& p = s.rec.stats.passes[static_cast<size_t>(i)];
      if (p.name == "morph") out.ms = p.gpu_ms;
    }
  }
  return out;
}

gr::FrameView eye(double x, double y, double z, double heading, double pitch, double sun_el) {
  gr::FrameView v;
  v.eye[0] = x;
  v.eye[1] = y;
  v.eye[2] = z;
  v.heading = heading;
  v.pitch = pitch;
  v.fov_y = 0.7;
  v.sun_elevation = sun_el;
  v.sun_azimuth = 2.4;
  v.altitude_m = 300;
  return v;
}

// A runner at (x, z) facing `heading`, at run phase `phase` (frames, any real), team `t`.
gr::MorphDraw runner_at(double x, double z, double heading, double phase, uint32_t t,
                        bool smooth = true) {
  gr::MorphDraw d;
  const double h = heading / 2;
  d.now.position[0] = x;
  d.now.position[2] = z;
  d.now.rotation[1] = static_cast<float>(libm::sin(h));
  d.now.rotation[3] = static_cast<float>(libm::cos(h));
  const double f = phase - kFrames * static_cast<double>(static_cast<int64_t>(phase / kFrames));
  d.now.frame_a = static_cast<uint32_t>(f);
  d.now.frame_b = (d.now.frame_a + 1) % kFrames;
  d.now.blend = static_cast<float>(f - d.now.frame_a);
  d.last = d.now;
  d.tint[0] = 2 * t;
  d.tint[1] = 2 * t + 1;
  d.smooth = smooth;
  return d;
}

// The mean colour of a picture's pixels inside a box (x0, y0, x1, y1).
void mean_rgb(const gl::Picture& p, int x0, int y0, int x1, int y1, double out[3]) {
  double sum[3] = {0, 0, 0};
  int n = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(p.width) + x) * 4;
      for (int k = 0; k < 3; ++k) sum[k] += p.rgba[i + static_cast<size_t>(k)];
      ++n;
    }
  }
  for (int k = 0; k < 3; ++k) out[k] = n > 0 ? sum[k] / n : 0;
}

void run(gl::Lab& lab) {
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = static_cast<uint32_t>(lab.arg_num("height"));
  const std::vector<gr::MorphModel> models = {runner()};
  gr::TintRamps ramps;
  for (const Team& t : kTeams) {
    gr::ramp_from(ramps, t.shirt);
    gr::ramp_from(ramps, t.trousers);
  }
  lab.metric("runner.triangles", static_cast<double>(models[0].indices.size()) / 3, "count");
  lab.metric("runner.frames", kFrames, "count");
  auto device = ge::gpu::create_device({.label = "gpu-morph", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, {"teams", "sharp-smooth", "frames", "dusk", "indoor"}, w, h);
    return;
  }
  Scene s;
  if (!make_scene(*device, s, w, h, models, ramps)) {
    lab.fail("the morph pass's pipelines did not build or its skins did not fit",
             "Read the gpu: lines above. Skill: render.");
    return;
  }

  // Four teams in a row, facing the camera, mid-stride.
  std::vector<gr::MorphDraw> team;
  for (uint32_t t = 0; t < 4; ++t) {
    team.push_back(runner_at(-1.8 + 1.2 * t, 4, 3.141592653589793 - 0.35, 1.5 + 2 * t, t));
  }
  const gl::Picture day = draw(*device, s, eye(0, 1.3, -1.5, 0, -0.04, 0.7), team).pic;
  lab.shot({"teams",
            "FOUR TEAMS, ONE SKIN",
            "One runner's model and skin drawn four times by day, each in its team's two ramps "
            "(the shirt and sleeves region A, the trousers region B), at four points of the run.",
            "red, blue, green and gold shirts each shaded down the body with a light chest band; "
            "trousers darker; the faces and hair the same on all four",
            {{"view", "day, 4 m"}}},
           day, Det::measured);
  // Each shirt's colour, sampled at its chest: the team's own channel leads.
  for (uint32_t t = 0; t < 4; ++t) {
    double rgb[3];
    const int cx = static_cast<int>(w * (0.5 + (-1.8 + 1.2 * t) / 5.6 * 0.95));
    mean_rgb(day, cx - 2, static_cast<int>(h * 0.36), cx + 2, static_cast<int>(h * 0.42), rgb);
    lab.metric(std::string("teams.") + kTeams[t].name + ".red", rgb[0], "level", Det::measured);
    lab.metric(std::string("teams.") + kTeams[t].name + ".blue", rgb[2], "level", Det::measured);
  }

  // Sharp and smooth, close.
  {
    std::vector<gr::MorphDraw> two = {runner_at(-0.33, 1.6, 3.141592653589793, 2.0, 1, false),
                                      runner_at(0.33, 1.6, 3.141592653589793, 2.0, 1, true)};
    lab.shot({"sharp-smooth",
              "SHARP (LEFT) AND SMOOTH (RIGHT)",
              "The same runner close, its skin read sharp (each texel a square: the classic look) "
              "and smooth (bilinear between texels each resolved to its tint first).",
              "blocky texels on the left; soft gradients on the right with no colour fringe "
              "where the shirt meets the face",
              {{"view", "1.6 m"}}},
             draw(*device, s, eye(0, 1.3, 0, 0, -0.1, 0.7), two).pic, Det::measured);
  }

  // The run's frames and the frames between them: a quarter-step filmstrip from the side.
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (int q = 0; q <= 8; ++q) {
      const double phase = 0.5 * q;
      const std::vector<gr::MorphDraw> one = {runner_at(0, 3, 1.5707963267948966, phase, 3)};
      gr::FrameView v = eye(-0.2, 1.0, 0, 0, -0.05, 0.7);
      pics.push_back(draw(*device, s, v, one).pic);
      names.push_back("FRAME " + fixed(phase, 2));
    }
    lab.shot({"frames",
              "THE RUN, FRAMES AND IN-BETWEENS",
              "One runner from the side at every half frame of its run: whole frames 0 to 4 and "
              "the frames blended half way between them on the GPU.",
              "the legs and arms swinging smoothly through each in-between, never jumping",
              {{"frames", "8 a cycle"}}},
             es::sheet_of(pics, names, 3), Det::measured);
  }

  // Dusk: the eyes glow.
  const gl::Picture dusk = draw(*device, s, eye(0, 1.5, 1.2, 0, -0.03, -0.07), team).pic;
  lab.shot({"dusk",
            "THE TEAMS AT DUSK",
            "The four runners after sunset, lit by the twilight sky: their glowing eye texels "
            "light themselves.",
            "dim figures, the shirts' colours still telling the teams apart, small bright eyes",
            {{"view", "dusk, 2.8 m"}}},
           dusk, Det::measured);

  // Indoors: no sun or sky, a game's own light.
  {
    std::vector<gr::MorphDraw> lit = {runner_at(0, 3, 3.141592653589793, 1, 0)};
    lit[0].outdoor = 0;
    for (int k = 0; k < 3; ++k) lit[0].ambient[k] = 0.15f;
    lit[0].shade[0] = 2.2f;
    lit[0].shade[1] = 1.6f;
    lit[0].shade[2] = 1.0f;
    lit[0].shade_dir[0] = 0.8f;
    lit[0].shade_dir[1] = 0.6f;
    lit[0].shade_dir[2] = 0;
    lab.shot({"indoor",
              "A RUNNER IN A GAME'S OWN LIGHT",
              "Outdoor share 0: no sun or sky on the runner, only an ambient and a warm shade "
              "from up and to the right, as a game samples its map's light where a figure "
              "stands.",
              "the runner's right side (screen left) warm and bright, its left side in a dim "
              "ambient; the ground still in daylight",
              {{"view", "3 m"}}},
             draw(*device, s, eye(0, 1.2, 0, 0, -0.03, 0.7), lit).pic, Det::measured);
  }

  // The cost: 64 runners in a crowd.
  {
    std::vector<gr::MorphDraw> crowd;
    crowd.reserve(64);
    for (int i = 0; i < 64; ++i) {
      const int row = i / 8;
      crowd.push_back(runner_at(-6 + (i % 8) * 1.7, 6 + row * 1.8, 3.141592653589793, i * 0.37,
                                static_cast<uint32_t>(i % 4)));
    }
    double ms = 0;
    for (int k = 0; k < 3; ++k) ms = draw(*device, s, eye(0, 3, 0, 0, -0.2, 0.7), crowd).ms;
    lab.metric("crowd.morph-gpu-ms", ms, "ms", Det::measured);
  }

  const uint32_t errors = device->errors.load();
  const uint32_t late = device->pipelines_after_load;
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above. Skill: render.");
  }
  if (late != 0) {
    lab.fail(std::to_string(late) + " pipelines made after the load",
             "Every pipeline belongs in create_morph. Skill: render.");
  }
}

const gl::LabSpec kSpec{
    "gpu-morph",
    gl::LabKind::picture,
    "engine",
    "engine/render/morph.hpp",
    "render",
    "Do vertex-animated models draw in their teams' colours, sharp or smooth, blend smoothly "
    "between their frames, glow where their skin says, and take a game's own light?",
    {
        {"width", "640", "each view's width, px"},
        {"height", "360", "each view's height, px"},
    },
    "--width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
