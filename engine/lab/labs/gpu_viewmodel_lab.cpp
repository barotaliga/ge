// LAB `gpu-viewmodel`: THE VIEW-MODEL PASS (render/viewmodel.hpp) on the software adapter, held
// to its CPU reference (viewmodel/raster.hpp). A weapon of boxes with a skin (steel, a wooden
// grip, a glowing sight dot), a recoil frame and a muzzle flash of its own the pass hides. Named
// views: the gun alone under each of the stage's six lights (viewmodel/stage.hpp) drawn by the CPU
// and by the GPU's resolved twin, with their difference; the same lights handed to the frame as
// clustered lamps; the GPU's gun over the CPU's corridor beside the CPU's own frame; and the whole
// frame outdoors, in the sun, in an awning's shadow, at night by a lamp and pressed against a wall.
// Checks: the GPU within the stated difference of the CPU, the shade darker than the sun, the gun
// whole against the wall, no GPU error, no pipeline after the load. Measured: the pass's GPU time.
// Without an adapter it notes that, with grey cards.
//
// ge:summary Lab: the view-model pass beside its CPU reference, by lamps, outdoors, in shadow
// ge:keywords lab gpu viewmodel view-model held weapon reference compare difference lamps
// ge:keywords clustered sun-shadow awning outdoor night guard wall depth-range
// ge:skill render labs
// ge:test tests/render_viewmodel_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "camera/pose.hpp"
#include "camera/to_view.hpp"
#include "core/math.hpp"
#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/labs/lit_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/raster.hpp"
#include "render/shadows.hpp"
#include "render/viewmodel.hpp"
#include "viewmodel/held.hpp"
#include "viewmodel/place.hpp"
#include "viewmodel/raster.hpp"
#include "viewmodel/stage.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace vm = ge::viewmodel;
namespace ls = ge::lab::lit_scene;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::placeholders;
using gl::Det;

// ── The gun, built in code ──

constexpr int kSkin = 16;
// The skin's regions: steel (rows 0..7), the wooden grip (rows 8..15, left half), the sight
// (rows 8..15, right half, its middle texel glowing).
struct Region {
  float u0, v0, u1, v1;
};
// Each is inset by half a texel, so a face's edge reads its own region, never the one beside it.
constexpr float kHalf = 0.5f / kSkin;
constexpr Region kSteel{kHalf, kHalf, 1 - kHalf, 0.5f - kHalf},
    kWood{kHalf, 0.5f + kHalf, 0.5f - kHalf, 1 - kHalf},
    kSight{0.5f + kHalf, 0.5f + kHalf, 1 - kHalf, 1 - kHalf};

vm::Skin gun_skin() {
  vm::Skin s;
  s.image.width = s.image.height = kSkin;
  s.image.rgba.assign(size_t{kSkin} * kSkin * 4, 255);
  s.glow.assign(size_t{kSkin} * kSkin, 0);
  for (int y = 0; y < kSkin; ++y) {
    for (int x = 0; x < kSkin; ++x) {
      uint8_t* t = &s.image.rgba[(size_t(y) * kSkin + size_t(x)) * 4];
      if (y < 8) {  // brushed steel: bands across
        const uint8_t v = static_cast<uint8_t>(118 + ((x * 5 + y * 3) % 7) * 4 - (y == 0 ? 20 : 0));
        t[0] = v;
        t[1] = static_cast<uint8_t>(v + 4);
        t[2] = static_cast<uint8_t>(v + 10);
      } else if (x < 8) {  // walnut, its grain down the grip
        const int g = (x * 7) % 5;
        t[0] = static_cast<uint8_t>(112 + g * 6);
        t[1] = static_cast<uint8_t>(70 + g * 4);
        t[2] = static_cast<uint8_t>(40 + g * 2);
      } else {  // the sight's dark post, a green dot in its middle
        t[0] = t[1] = t[2] = 40;
        if (y >= 11 && y <= 12 && x >= 11 && x <= 12) {
          t[0] = 90;
          t[1] = 255;
          t[2] = 120;
          s.glow[size_t(y) * kSkin + size_t(x)] = 1;
        }
      }
    }
  }
  return s;
}

// A weapon of skinned boxes in the held frame, two frames (rest; the receiver back in recoil) and
// a muzzle flash of its own (two crossed quads, marked in `flame`).
vm::HeldModel skinned_gun() {
  vm::HeldModel m;
  m.frames.resize(2);
  auto box = [&](const float lo[3], const float hi[3], Region r, float recoil) {
    const float v[8][3] = {{lo[0], lo[1], lo[2]}, {hi[0], lo[1], lo[2]}, {hi[0], hi[1], lo[2]},
                           {lo[0], hi[1], lo[2]}, {lo[0], lo[1], hi[2]}, {hi[0], lo[1], hi[2]},
                           {hi[0], hi[1], hi[2]}, {lo[0], hi[1], hi[2]}};
    const int faces[6][4] = {{1, 2, 6, 5}, {0, 4, 7, 3}, {3, 7, 6, 2},
                             {0, 1, 5, 4}, {4, 5, 6, 7}, {0, 3, 2, 1}};
    const float normals[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                 {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    const float uv[4][2] = {{r.u0, r.v1}, {r.u1, r.v1}, {r.u1, r.v0}, {r.u0, r.v0}};
    for (int fi = 0; fi < 6; ++fi) {
      const uint32_t base = static_cast<uint32_t>(m.frames[0].positions.size() / 3);
      for (int c = 0; c < 4; ++c) {
        const float* p = v[faces[fi][c]];
        for (int f = 0; f < 2; ++f) {
          const float z = p[2] - (f == 1 ? recoil : 0.0f);
          m.frames[size_t(f)].positions.insert(m.frames[size_t(f)].positions.end(),
                                               {p[0], p[1], z});
          m.frames[size_t(f)].normals.insert(m.frames[size_t(f)].normals.end(), normals[fi],
                                             normals[fi] + 3);
        }
        m.uv.insert(m.uv.end(), {uv[c][0], uv[c][1]});
      }
      const float* a = v[faces[fi][0]];
      const float* b = v[faces[fi][1]];
      const float* c = v[faces[fi][2]];
      const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      const float cr[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                           e1[0] * e2[1] - e1[1] * e2[0]};
      if (cr[0] * normals[fi][0] + cr[1] * normals[fi][1] + cr[2] * normals[fi][2] >= 0)
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
      else
        m.indices.insert(m.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
      m.flame.insert(m.flame.end(), {0, 0});
    }
  };
  const float rl[3] = {-0.03f, -0.02f, -0.12f}, rh[3] = {0.03f, 0.06f, 0.18f};
  const float bl[3] = {-0.015f, 0.01f, 0.18f}, bh[3] = {0.015f, 0.04f, 0.46f};
  const float gl0[3] = {-0.02f, -0.14f, -0.06f}, gh[3] = {0.02f, -0.02f, 0.0f};
  const float sl[3] = {-0.008f, 0.06f, 0.10f}, sh[3] = {0.008f, 0.085f, 0.14f};
  box(rl, rh, kSteel, 0.03f);  // the receiver, back 3 cm in recoil
  box(bl, bh, kSteel, 0.0f);   // the barrel
  box(gl0, gh, kWood, 0.0f);   // the grip
  box(sl, sh, kSight, 0.03f);  // the sight, on the receiver
  // The model's own flash: a cross of quads at the muzzle, both sides (the pass hides it).
  const float cz = 0.47f, cy = 0.025f, r = 0.05f;
  const float quads[4][4][3] = {
      {{-r, cy, cz}, {r, cy, cz}, {r, cy, cz + 0.08f}, {-r, cy, cz + 0.08f}},
      {{-r, cy, cz + 0.08f}, {r, cy, cz + 0.08f}, {r, cy, cz}, {-r, cy, cz}},
      {{0, cy - r, cz}, {0, cy + r, cz}, {0, cy + r, cz + 0.08f}, {0, cy - r, cz + 0.08f}},
      {{0, cy - r, cz + 0.08f}, {0, cy + r, cz + 0.08f}, {0, cy + r, cz}, {0, cy - r, cz}}};
  for (const auto& q : quads) {
    const uint32_t base = static_cast<uint32_t>(m.frames[0].positions.size() / 3);
    for (const auto& p : q) {
      for (int f = 0; f < 2; ++f) {
        m.frames[size_t(f)].positions.insert(m.frames[size_t(f)].positions.end(), p, p + 3);
        m.frames[size_t(f)].normals.insert(m.frames[size_t(f)].normals.end(), {0, 0, -1});
      }
      m.uv.insert(m.uv.end(), {0.75f, 0.72f});  // the glowing dot: bright if ever drawn
    }
    m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    m.flame.insert(m.flame.end(), {1, 1});
  }
  m.skin = gun_skin();
  return m;
}

// ── The scene: the lit labs' flagstones and props, the sun's shadows and the held gun ──

struct Scene : ls::Scene {
  gr::SunShadows shadows;
  gr::HeldModels held;
  ge::gpu::Target resolved;
  wgpu::Texture resolved_depth;
  wgpu::TextureView resolved_depth_view;
};

// Props: an awning (a roof slab on four posts) round (kCx + 20, kCz), a wall at (kCx - 20, kCz).
constexpr double kAwningX = ls::kCx + 20, kWallX = ls::kCx - 20;

std::vector<gr::Instance> props() {
  std::vector<gr::Instance> p;
  p.push_back(ls::at(kAwningX, 2.8, ls::kCz, 9, 0.2f, 9, 0, 0));
  for (const double dx : {-4.2, 4.2})
    for (const double dz : {-4.2, 4.2})
      p.push_back(ls::at(kAwningX + dx, 0, ls::kCz + dz, 0.2f, 2.8f, 0.2f, 0, 1));
  p.push_back(ls::at(kWallX, 0, ls::kCz + 0.55, 6, 3, 0.4f, 0, 0));
  return p;
}

bool make(ge::gpu::Device& d, Scene& s, uint32_t w, uint32_t h, const vm::HeldModel& gun) {
  // The lit labs' scene with the sun's shadows, the held gun's pipelines made before the load
  // closes.
  const gr::Surface surfaces[] = {gr::Surface{.albedo = {0.42f, 0.4f, 0.38f}},
                                  gr::Surface{.albedo = {0.25f, 0.2f, 0.16f}}};
  const vm::HeldModel models[] = {gun};
  auto more = [&] {
    gr::ShadowSettings shadow;
    shadow.size = 1024;
    gr::create_sun_shadows(d, s.shadows, shadow, s.layout, s.sky);
    gr::HeldSettings settings;
    settings.resolved = true;
    return gr::create_viewmodel(d, s.pipelines, s.held, s.layout, models, settings);
  };
  if (!ls::make_scene(d, s, surfaces, props(), w, h, "gpu-viewmodel", more, gr::kCascades)) {
    return false;
  }
  gr::bind_instance_shadows(s.ip, s.shadows.group);
  gr::bind_viewmodel_shadows(s.held, s.shadows.group);
  s.resolved = ge::gpu::create_target(d, "gpu-viewmodel.resolved", w, h);
  s.resolved_depth = gr::make_texture(d, "gpu-viewmodel.resolved-depth", w, h, 1, gr::kDepthFormat,
                                      wgpu::TextureUsage::RenderAttachment);
  s.resolved_depth_view = s.resolved_depth.CreateView();
  return true;
}

struct Drawn {
  gl::Picture pic;
  double ms = std::numeric_limits<double>::quiet_NaN();  // the pass on the GPU
};

// The whole frame: the courtyard, the sky, the shadows, then the gun, then post.
Drawn frame(ge::gpu::Device& d, Scene& s, const gr::FrameView& v,
            std::span<const gr::LocalLight> lamps, std::span<const gr::HeldDraw> draws) {
  s.rec.begin_frame(d);
  gr::fill_uniforms(s.staged, v, s.w, s.h);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  gr::prepare_sky(s.sky, v);
  gr::prepare_terrain(d, s.terrain, v);
  gr::prepare_family(d, s.props, v, s.h, false);
  gr::InstanceFamily* families[] = {&s.props};
  gr::reset_sun_shadows(s.shadows);
  gr::prepare_sun_shadows(d, s.shadows, v, {}, families, {});
  gr::prepare_lights(d, s.lights, lamps, v, s.w, s.h);
  gr::prepare_viewmodel(d, s.held, draws, v, s.w, s.h);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  gr::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  gr::encode_instances_cull(families, s.ip, s.pipelines, s.rec, e, s.frame, false);
  gr::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  gr::encode_sun_shadows(s.shadows, families, {}, s.ip, s.pipelines, s.rec, e);
  gr::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_instances_depth(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light,
                             false);
  gr::encode_hiz(s.hiz, s.pipelines, s.rec, e);
  gr::encode_instances_cull(families, s.ip, s.pipelines, s.rec, e, s.frame, true);
  gr::encode_instances_depth(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light,
                             true);
  gr::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_instances(families, s.ip, s.pipelines, s.rec, e, s.targets, s.frame, s.light);
  gr::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_viewmodel(s.held, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, s.out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, s.out);
  s.rec.wait(d);
  Drawn out{gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba}};
  if (s.rec.timed) {
    for (int i = 0; i < s.rec.stats.count; ++i) {
      const gr::PassStat& p = s.rec.stats.passes[static_cast<size_t>(i)];
      if (p.name == "viewmodel") out.ms = p.gpu_ms;
    }
  }
  return out;
}

// The gun alone, resolved as the CPU reference resolves it (alpha 0 where it drew nothing).
gl::Picture resolved(ge::gpu::Device& d, Scene& s, const gr::FrameView& v,
                     std::span<const gr::LocalLight> lamps, const gr::HeldDraw& draw) {
  s.rec.begin_frame(d);
  gr::fill_uniforms(s.staged, v, s.w, s.h);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  gr::prepare_sky(s.sky, v);
  gr::prepare_lights(d, s.lights, lamps, v, s.w, s.h);
  gr::prepare_viewmodel(d, s.held, std::span<const gr::HeldDraw>(&draw, 1), v, s.w, s.h);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  gr::encode_viewmodel_resolved(s.held, s.pipelines, s.rec, e, s.resolved.view,
                                s.resolved_depth_view, s.w, s.h, s.frame);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, s.resolved);
  s.rec.wait(d);
  return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
}

// The CPU reference: the gun alone on black, its coverage in alpha.
gl::Picture reference(const vm::HeldModel& gun, const vm::Placement& place,
                      const vm::Lighting& light, double fov_y, int w, int h) {
  const float black[3] = {0, 0, 0};
  vm::ViewTarget t = vm::make_target(w, h, 1, black);
  std::vector<float> p, n;
  vm::place_held(gun, place, 0, 0, 0, p, n);
  vm::DrawLit g;
  g.positions = p;
  g.normals = n;
  g.indices = gun.indices;
  g.uv = gun.uv;
  g.skin = &gun.skin;
  g.fov_y = fov_y;
  g.near_m = gr::kHeldNearM;
  g.hidden = gun.flame;
  vm::draw_lit(t, g, light);
  gl::Picture out{w, h, vm::resolve(t, 1.0)};
  for (size_t i = 0; i < t.depth.size(); ++i)
    out.rgba[i * 4 + 3] = t.depth[i] < std::numeric_limits<float>::infinity() ? 255 : 0;
  return out;
}

// How far two gun pictures are apart: over the pixels both drew, the mean and the 99th centile of
// the largest channel's difference (levels); the share of the pixels either drew that only one did.
struct Diff {
  double mean = 0, p99 = 0, coverage = 0;
  gl::Picture heat;  // the difference times 8, the coverage's mismatch in magenta
};
Diff compare(const gl::Picture& a, const gl::Picture& b) {
  Diff d;
  d.heat = gl::create_picture(a.width, a.height, {0, 0, 0});
  std::vector<int> diffs;
  size_t either = 0, one = 0;
  for (size_t i = 0; i < size_t(a.width) * size_t(a.height); ++i) {
    const bool ia = a.rgba[i * 4 + 3] > 127, ib = b.rgba[i * 4 + 3] > 127;
    if (ia || ib) ++either;
    uint8_t* hp = &d.heat.rgba[i * 4];
    if (ia != ib) {
      ++one;
      hp[0] = 255;
      hp[2] = 255;
      continue;
    }
    if (!ia) continue;
    int most = 0;
    for (int k = 0; k < 3; ++k)
      most =
          std::max(most, std::abs(int(a.rgba[i * 4 + size_t(k)]) - int(b.rgba[i * 4 + size_t(k)])));
    diffs.push_back(most);
    hp[0] = hp[1] = hp[2] = static_cast<uint8_t>(std::min(255, most * 8));
  }
  if (!diffs.empty()) {
    double sum = 0;
    for (int v : diffs) sum += v;
    d.mean = sum / static_cast<double>(diffs.size());
    std::sort(diffs.begin(), diffs.end());
    d.p99 = diffs[std::min(diffs.size() - 1, diffs.size() * 99 / 100)];
  }
  d.coverage = either > 0 ? static_cast<double>(one) / static_cast<double>(either) : 0;
  return d;
}

// The stated difference the GPU is held to, against the CPU reference (levels of 255): the light is
// the same arithmetic in floats, so the mean is under a level; the 99th centile is where a pixel's
// centre falls on the other side of a skin texel's edge or a triangle's (one texel's step).
constexpr double kMeanMost = 1.0, kP99Most = 16.0, kCoverageMost = 0.02;

// Panels laid `cols` across, each with its label.
gl::Picture sheet(const std::vector<gl::Picture>& panels, const std::vector<std::string>& labels,
                  int cols) {
  const int w = panels[0].width, h = panels[0].height, gap = 4;
  const int rows = (static_cast<int>(panels.size()) + cols - 1) / cols;
  gl::Picture out =
      gl::create_picture(cols * (w + gap) + gap, rows * (h + gap) + gap, {16, 17, 20});
  for (size_t i = 0; i < panels.size(); ++i) {
    const int x = gap + static_cast<int>(i % size_t(cols)) * (w + gap);
    const int y = gap + static_cast<int>(i / size_t(cols)) * (h + gap);
    gl::blit(out, panels[i], x, y);
    gl::fill_rect(out, x, y, gl::text_width(labels[i], 1) + 6, 12, {0, 0, 0, 150});
    gl::draw_text(out, x + 3, y + 2, labels[i], {240, 240, 240}, 1);
  }
  return out;
}

ge::camera::Pose stage_view() {
  ge::camera::Pose v;
  v.eye = {0, 1.6, 0};
  v.pitch = -0.04;
  v.fov_y = ge::camera::fov_y_of(90.0 * ge::core::kPi / 180.0, 16.0 / 9.0);
  return v;
}

gl::Picture over(const gl::Picture& under, const gl::Picture& gun) {
  gl::Picture out = under;
  for (size_t i = 0; i < size_t(gun.width) * size_t(gun.height); ++i)
    if (gun.rgba[i * 4 + 3] > 127)
      for (int k = 0; k < 3; ++k) out.rgba[i * 4 + size_t(k)] = gun.rgba[i * 4 + size_t(k)];
  return out;
}

double mean_luma(const gl::Picture& p, int x0, int y0, int x1, int y1) {
  double sum = 0;
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      const uint8_t* c = &p.rgba[(size_t(y) * size_t(p.width) + size_t(x)) * 4];
      sum += 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
      ++n;
    }
  return n > 0 ? sum / n : 0;
}

void run(gl::Lab& lab) {
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = static_cast<uint32_t>(lab.arg_num("height"));
  const vm::HeldModel gun = skinned_gun();
  lab.metric("gun.triangles", static_cast<double>(gun.indices.size()) / 3, "count");
  auto device = ge::gpu::create_device({.label = "gpu-viewmodel", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab, {"reference", "lamps", "corridor", "outdoor"}, w, h);
    return;
  }
  ge::gpu::Device& d = *device;
  Scene s;
  if (!make(d, s, w, h, gun)) {
    lab.fail("the view-model pass's pipelines did not build or its skin did not fit",
             "Read the gpu: lines above. Skill: render.");
    return;
  }
  const int iw = static_cast<int>(w), ih = static_cast<int>(h);

  // The gun at rest on the classic row, in the stage's view.
  const ge::camera::Pose view = stage_view();
  gr::FrameView fv;
  ge::camera::to_frame_view(view, fv);
  vm::PlaceState st;
  const vm::Placement rest = vm::frame_viewmodel(vm::classic_row(), vm::PlaceInput{}, st, 1.0 / 60);
  const double gun_fov = rest.fov_y > 0 ? rest.fov_y : view.fov_y;
  const float grip[3] = {static_cast<float>(rest.m[3]), static_cast<float>(rest.m[7]),
                         static_cast<float>(rest.m[11])};
  float muzzle[3];
  {
    float tip[3];
    vm::muzzle_at(gun, 0, 0, 0, tip);
    vm::place_point(rest, tip, muzzle);
  }
  constexpr double kWhiteLux = 1000;  // what the held unit's 1 is on the frame (cancels resolved)
  auto held_draw = [&](const vm::Lighting& light) {
    gr::HeldDraw g;
    g.now = g.last = rest;
    g.fov_y = gun_fov;
    g.light = light;
    g.white_lux = kWhiteLux;
    return g;
  };

  // 1. Each light, held lights: the CPU, the GPU and their difference.
  {
    std::vector<gl::Picture> panels;
    std::vector<std::string> labels;
    double worst_mean = 0, worst_p99 = 0, worst_cov = 0;
    for (vm::StageLight kind : vm::kStageLights) {
      const vm::StageLights l = vm::stage_lights(kind, view, muzzle);
      const vm::Lighting light = vm::light_held(l.probe, l.lights, view, grip, l.floor);
      const gl::Picture cpu = reference(gun, rest, light, gun_fov, iw, ih);
      const gl::Picture gpu = resolved(d, s, fv, {}, held_draw(light));
      const Diff diff = compare(cpu, gpu);
      const std::string name(vm::stage_light_name(kind));
      lab.metric("reference." + name + ".mean", diff.mean, "levels", Det::measured);
      lab.metric("reference." + name + ".p99", diff.p99, "levels", Det::measured);
      lab.metric("reference." + name + ".coverage", diff.coverage, "share", Det::measured);
      worst_mean = std::max(worst_mean, diff.mean);
      worst_p99 = std::max(worst_p99, diff.p99);
      worst_cov = std::max(worst_cov, diff.coverage);
      panels.insert(panels.end(), {cpu, gpu, diff.heat});
      labels.insert(labels.end(),
                    {"CPU " + name, "GPU " + name, "DIFF x8 MEAN " + fixed(diff.mean, 2)});
    }
    lab.shot(
        {"reference",
         "THE GPU BESIDE ITS CPU REFERENCE",
         "The weapon alone under each of the stage's six lights (day, dim, dark, a torch, its "
         "own muzzle flash, a red and a blue lamp), lit by the lights light_held kept: the CPU "
         "raster, the GPU pass's resolved twin, and their difference times eight (magenta "
         "where only one drew).",
         "the CPU and GPU columns alike; the difference black but for a thin outline; the "
         "sight's dot glowing in the dark; no muzzle flash drawn",
         {{"stated", "mean <= " + fixed(kMeanMost, 1) + ", p99 <= " + fixed(kP99Most, 0) +
                         " levels, coverage <= " + fixed(kCoverageMost * 100, 0) + "%"}}},
        sheet(panels, labels, 3), Det::measured);
    lab.metric("reference.worst-mean", worst_mean, "levels", Det::measured);
    lab.metric("reference.worst-p99", worst_p99, "levels", Det::measured);
    lab.metric("reference.worst-coverage", worst_cov, "share", Det::measured);
    if (worst_mean > kMeanMost || worst_p99 > kP99Most || worst_cov > kCoverageMost) {
      lab.fail("the GPU pass is past the stated difference from the CPU reference (mean " +
                   fixed(worst_mean, 2) + ", p99 " + fixed(worst_p99, 0) + ", coverage " +
                   fixed(worst_cov, 3) + ")",
               "Compare render/viewmodel.wgsl's held_colour with viewmodel/light.cpp's shade_held "
               "and the projection with viewmodel/raster.cpp's. Skill: render.");
    }
  }

  // 2. The same lights as the frame's clustered lamps.
  {
    std::vector<gl::Picture> panels;
    std::vector<std::string> labels;
    double worst = 0;
    for (vm::StageLight kind :
         {vm::StageLight::torch, vm::StageLight::flash, vm::StageLight::coloured}) {
      const vm::StageLights l = vm::stage_lights(kind, view, muzzle);
      const vm::Lighting light = vm::light_held(l.probe, l.lights, view, grip, l.floor);
      vm::Lighting bare = vm::light_held(l.probe, {}, view, grip, l.floor);
      std::vector<gr::LocalLight> lamps;
      for (const vm::PointLight& p : l.lights) lamps.push_back(gr::lamp_of(p, kWhiteLux));
      const gl::Picture cpu = reference(gun, rest, light, gun_fov, iw, ih);
      const gl::Picture gpu = resolved(d, s, fv, lamps, held_draw(bare));
      const Diff diff = compare(cpu, gpu);
      const std::string name(vm::stage_light_name(kind));
      lab.metric("lamps." + name + ".mean", diff.mean, "levels", Det::measured);
      worst = std::max(worst, diff.mean);
      panels.insert(panels.end(), {cpu, gpu, diff.heat});
      labels.insert(labels.end(),
                    {"CPU " + name, "GPU LAMPS " + name, "DIFF x8 MEAN " + fixed(diff.mean, 2)});
    }
    lab.shot({"lamps",
              "THE STAGE'S LIGHTS AS THE FRAME'S LAMPS",
              "The torch, the muzzle flash and the two coloured lamps handed to the frame as "
              "clustered local lights (render/lights.hpp, their candela by held_candela) instead "
              "of the gun's own list: the GPU reads them from the cluster its pixel looks into.",
              "the GPU column as the CPU's: the torch's warm side, the flash's hot underside, red "
              "and blue on either side",
              {{"white", fixed(kWhiteLux, 0) + " lx"}}},
             sheet(panels, labels, 3), Det::measured);
    lab.metric("lamps.worst-mean", worst, "levels", Det::measured);
    if (worst > kMeanMost) {
      lab.fail(
          "the clustered lamps light the gun past the stated difference from the gun's own "
          "list (mean " +
              fixed(worst, 2) + ")",
          "Check lamp_of's candela and held_colour's lamp loop (the cluster at the gun's "
          "depth). Skill: render.");
    }
  }

  // 3. The GPU's gun over the CPU's corridor, beside the CPU's own frame.
  {
    vm::HeldModel none;
    none.frames.resize(1);
    vm::StageShot shot;
    shot.width = iw;
    shot.height = ih;
    shot.supersample = 1;
    std::vector<gl::Picture> panels;
    std::vector<std::string> labels;
    for (vm::StageLight kind :
         {vm::StageLight::day, vm::StageLight::torch, vm::StageLight::coloured}) {
      const vm::StageLights l = vm::stage_lights(kind, view, muzzle);
      const vm::Lighting light = vm::light_held(l.probe, l.lights, view, grip, l.floor);
      vm::HeldModel shown = gun;
      for (size_t t = 0; t < shown.flame.size(); ++t)
        if (shown.flame[t])
          shown.indices[t * 3 + 1] = shown.indices[t * 3 + 2] = shown.indices[t * 3];
      const gl::Picture cpu{iw, ih,
                            vm::draw_stage(vm::corridor_stage(), l, view, shown, rest, shot)};
      const gl::Picture room{iw, ih,
                             vm::draw_stage(vm::corridor_stage(), l, view, none, rest, shot)};
      panels.push_back(cpu);
      panels.push_back(over(room, resolved(d, s, fv, {}, held_draw(light))));
      const std::string name(vm::stage_light_name(kind));
      labels.insert(labels.end(), {"CPU " + name, "GPU " + name});
    }
    lab.shot({"corridor",
              "IN THE CORRIDOR",
              "The stage's corridor drawn by the CPU, the gun over it by the CPU (left) and by the "
              "GPU pass (right), by day, by a torch and between a red and a blue lamp.",
              "each pair the same picture",
              {{"row", "classic"}}},
             sheet(panels, labels, 2), Det::measured);
  }

  // 4. The whole frame outdoors: in the sun, under the awning, at night by a lamp, at a wall.
  {
    auto outdoors = [&](double x, double heading, bool night) {
      gr::FrameView v = ls::view_at(x, 1.6, ls::kCz, heading, -0.04, night, 2.0);
      v.fov_y = view.fov_y;
      return v;
    };
    vm::Lighting none;  // no probe: the frame's own light
    none.floor = 0.02f;
    gr::HeldDraw out_draw;
    out_draw.now = out_draw.last = rest;
    out_draw.fov_y = gun_fov;
    out_draw.light = none;
    out_draw.outdoor = 1;
    const std::vector<gr::HeldDraw> one = {out_draw};
    const std::vector<gr::HeldDraw> nothing;
    const gr::FrameView sun = outdoors(ls::kCx, 0, false);
    const gr::FrameView shade = outdoors(kAwningX, 0, false);
    gr::FrameView night = outdoors(ls::kCx, 0, true);
    const gr::FrameView wall = outdoors(kWallX, 0, false);
    gr::LocalLight lamp;
    lamp.position[0] = ls::kCx - 1.2;
    lamp.position[1] = 2.0;
    lamp.position[2] = ls::kCz + 1.0;
    lamp.colour[0] = 1.0f;
    lamp.colour[1] = 0.78f;
    lamp.colour[2] = 0.5f;
    lamp.candela = 60;
    lamp.range_m = 8;
    const std::vector<gr::LocalLight> lamps = {lamp};
    night.lamp_lux = gr::lamp_lux_at(lamps, night.eye, night.time_s);  // the eye adapts to it
    const Drawn in_sun = frame(d, s, sun, {}, one);
    const Drawn in_shade = frame(d, s, shade, {}, one);
    const Drawn at_night = frame(d, s, night, lamps, one);
    const Drawn at_wall = frame(d, s, wall, {}, one);
    const Drawn bare_wall = frame(d, s, wall, {}, nothing);
    lab.shot({"outdoor",
              "THE WHOLE FRAME",
              "The pass in a frame of the courtyard (the terrain, the props, the sky, the sun's "
              "shadows, post): the gun in the noon sun, under an awning's shadow, at night beside "
              "a warm lamp up and to the left, and pressed against a wall 0.35 m ahead.",
              "a bright gun in the sun; the same gun dim and cool under the awning; at night lit "
              "warm from the left; against the wall the gun whole in front of it",
              {{"probe", "none: outdoor 1, the frame's sun, sky and lamps"}}},
             sheet({in_sun.pic, in_shade.pic, at_night.pic, at_wall.pic},
                   {"SUN", "AWNING", "NIGHT LAMP", "WALL"}, 2),
             Det::measured);
    // The gun's own patch: low right of the picture.
    const int x0 = iw * 62 / 100, x1 = iw * 80 / 100, y0 = ih * 70 / 100, y1 = ih * 92 / 100;
    const double lit = mean_luma(in_sun.pic, x0, y0, x1, y1);
    const double shaded = mean_luma(in_shade.pic, x0, y0, x1, y1);
    lab.metric("outdoor.sun-luma", lit, "level", Det::measured);
    lab.metric("outdoor.shade-luma", shaded, "level", Det::measured);
    if (!(shaded < lit * 0.8)) {
      lab.fail("the gun under the awning is not darker than in the sun (" + fixed(shaded, 1) +
                   " against " + fixed(lit, 1) + ")",
               "Check the sun's shadow in held_colour and bind_viewmodel_shadows. Skill: render.");
    }
    const double moved =
        std::abs(mean_luma(at_wall.pic, x0, y0, x1, y1) - mean_luma(bare_wall.pic, x0, y0, x1, y1));
    lab.metric("outdoor.wall-gun-change", moved, "level", Det::measured);
    if (moved < 4) {
      lab.fail("against the wall the gun did not draw (its patch moved " + fixed(moved, 1) + ")",
               "Check held_depth_floor and the pass's viewport depth range. Skill: render.");
    }
    double ms = 0;
    for (int k = 0; k < 3; ++k) ms = frame(d, s, sun, {}, one).ms;
    lab.metric("viewmodel-gpu-ms", ms, "ms", Det::measured);
  }

  const uint32_t errors = d.errors.load();
  const uint32_t late = d.pipelines_after_load;
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above. Skill: render.");
  }
  if (late != 0) {
    lab.fail(std::to_string(late) + " pipelines made after the load",
             "Every pipeline belongs in create_viewmodel. Skill: render.");
  }
}

const gl::LabSpec kSpec{
    "gpu-viewmodel",
    gl::LabKind::picture,
    "engine",
    "engine/render/viewmodel.hpp",
    "render",
    "Does the view-model pass draw the held weapon as its CPU reference does, by the frame's "
    "lamps, in the sun and its shadows, and in front of a wall the player stands against?",
    {
        {"width", "480", "each view's width, px"},
        {"height", "270", "each view's height, px"},
    },
    "--width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
