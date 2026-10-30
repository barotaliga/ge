// LAB `gpu-sprites`: SPRITES (render/sprites.hpp) on the software adapter, every frame drawn in
// code: a bush (cut out, standing upright with the camera), a sign (cut out, fixed in the world
// by its own rotation), a torch's flame of six frames (added light, upright to the eye, glowing),
// a fireball of eight frames (added light, in the screen's plane, glowing), bubbles (laid over,
// in the screen's plane) and a spark (added, rolled in the screen's plane). Named views: all of
// them on a yard by day; the same from high above, where each facing rule shows (the bush and
// the flame stay upright, the fireball and the bubbles tilt with the camera, the sign keeps its
// place); sharp beside smooth up close; the fireball's frames as a filmstrip at dusk; the yard
// at dusk with only the glowing sprites bright; the yard at night under a lamp, off and on.
// Checks: the glowing sprites outshine the lit ones at dusk, the lamp lights the near bush, the
// cutout keeps the ground between its leaves, no GPU error, no pipeline after the load. Measured:
// the pass's GPU time for 2000 sprites. Without an adapter it notes that, with grey cards.
//
// ge:summary Lab: sprites by facing rule and blend, sharp and smooth, animated, a lamp, the cost
// ge:keywords lab gpu sprites sprite billboard facing parallel upright oriented cutout additive
// ge:keywords premultiplied glow fireball flame bush sign frames animation picture
// ge:keywords night lamps street-lamp
// ge:skill render labs
// ge:test tests/render_sprites_test.cpp
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
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
#include "render/lights.hpp"
#include "render/night.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/sprites.hpp"
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

// ── The frames, drawn in code ──

// A hash of three integers to 0..1.
double hash01(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  return (h & 0xFFFFFFu) / 16777216.0;
}

gr::SpriteFrame blank(uint32_t w, uint32_t h, float ox, float oy) {
  gr::SpriteFrame f;
  f.width = w;
  f.height = h;
  f.rgba.assign(size_t{w} * h * 4, 0);
  f.origin[0] = ox;
  f.origin[1] = oy;
  return f;
}

void put(gr::SpriteFrame& f, uint32_t x, uint32_t y, double r, double g, double b, double a) {
  auto byte = [](double v) {
    return static_cast<uint8_t>(v <= 0 ? 0 : v >= 1 ? 255 : std::lround(v * 255));
  };
  uint8_t* t = &f.rgba[(size_t{y} * f.width + x) * 4];
  t[0] = byte(r);
  t[1] = byte(g);
  t[2] = byte(b);
  t[3] = byte(a);
}

// A bush: a ragged dome of leaves, holes in it, its foot at the bottom middle.
gr::SpriteFrame bush() {
  gr::SpriteFrame f = blank(32, 32, 16, 32);
  for (uint32_t y = 0; y < 32; ++y) {
    for (uint32_t x = 0; x < 32; ++x) {
      const double dx = (x + 0.5 - 16) / 15, dy = (32 - y - 0.5) / 26;
      const double r = std::sqrt(dx * dx + dy * dy);
      const double leaf = hash01(x / 2, y / 2, 1);
      if (r > 1 - 0.25 * leaf || leaf < 0.12) continue;
      const double shade = 0.55 + 0.45 * (1 - static_cast<double>(y) / 32) * hash01(x, y, 2);
      put(f, x, y, 0.16 * shade, 0.42 * shade, 0.12 * shade, 1);
    }
  }
  return f;
}

// A sign: a board with a white arrow pointing right, on a post.
gr::SpriteFrame sign() {
  gr::SpriteFrame f = blank(32, 40, 16, 40);
  for (uint32_t y = 0; y < 40; ++y) {
    for (uint32_t x = 0; x < 32; ++x) {
      if (y >= 20) {
        if (x >= 14 && x < 18) put(f, x, y, 0.32, 0.22, 0.12, 1);
        continue;
      }
      const bool rim = x < 2 || x > 29 || y < 2 || y > 17;
      const int ay = static_cast<int>(y) - 10, ax = static_cast<int>(x);
      const bool shaft = ax >= 6 && ax < 19 && ay >= -2 && ay < 2;
      const bool head = ax >= 19 && ax < 27 && (ay < 0 ? -ay : ay) <= 26 - ax;
      if (rim) {
        put(f, x, y, 0.08, 0.08, 0.08, 1);
      } else if (shaft || head) {
        put(f, x, y, 0.95, 0.95, 0.92, 1);
      } else {
        put(f, x, y, 0.1, 0.32, 0.7, 1);
      }
    }
  }
  return f;
}

// A flame's frame `k` of 6: a tongue licking up, its foot at the bottom middle.
gr::SpriteFrame flame(uint32_t k) {
  gr::SpriteFrame f = blank(16, 32, 8, 32);
  for (uint32_t y = 0; y < 32; ++y) {
    for (uint32_t x = 0; x < 16; ++x) {
      const double up = (32 - y - 0.5) / 32;  // 0 at the foot
      const double sway = 1.6 * libm::sin(up * 5 + k * 1.05) * up;
      const double half = 6.5 * (1 - up) * (0.6 + 0.4 * up) + 0.5;
      const double d = (x + 0.5 - 8 - sway) / half;
      const double flick = hash01(x, y / 3 + k * 5, 3);
      const double a = (1 - d * d) * (1 - up * up) * (0.8 + 0.4 * flick);
      if (a <= 0) continue;
      const double hot = a > 0.6 ? 1 : a / 0.6;
      put(f, x, y, 1, 0.35 + 0.6 * hot, 0.08 + 0.5 * hot * hot, a > 1 ? 1 : a);
    }
  }
  return f;
}

// A fireball's frame `k` of 8: a ball swelling, cooling to smoke and thinning.
gr::SpriteFrame fireball(uint32_t k) {
  gr::SpriteFrame f = blank(32, 32, 16, 16);
  const double t = k / 7.0;
  for (uint32_t y = 0; y < 32; ++y) {
    for (uint32_t x = 0; x < 32; ++x) {
      const double dx = (x + 0.5 - 16) / 15, dy = (y + 0.5 - 16) / 15;
      const double r = std::sqrt(dx * dx + dy * dy) / (0.45 + 0.55 * t);
      const double lump = hash01(x / 3, y / 3, 10 + k);
      const double a = (1 - r * r) * (1 - t * 0.85) * (0.75 + 0.5 * lump);
      if (a <= 0) continue;
      const double heat = (1 - t) * (1 - r);
      put(f, x, y, 0.35 + 0.65 * (heat * 2 > 1 ? 1 : heat * 2), 0.18 + 0.8 * heat,
          0.08 + 0.6 * heat * heat, a > 1 ? 1 : a);
    }
  }
  return f;
}

// A bubble: a thin ring with a highlight.
gr::SpriteFrame bubble() {
  gr::SpriteFrame f = blank(12, 12, 6, 6);
  for (uint32_t y = 0; y < 12; ++y) {
    for (uint32_t x = 0; x < 12; ++x) {
      const double dx = (x + 0.5 - 6) / 5.5, dy = (y + 0.5 - 6) / 5.5;
      const double r = std::sqrt(dx * dx + dy * dy);
      if (r > 1) continue;
      const double ring = r > 0.75 ? 0.8 : 0.15;
      const bool glint = dx < -0.2 && dx > -0.6 && dy < -0.2 && dy > -0.6;
      put(f, x, y, 0.75, 0.88, 1, glint ? 0.95 : ring);
    }
  }
  return f;
}

// A spark: a four-pointed star.
gr::SpriteFrame spark() {
  gr::SpriteFrame f = blank(16, 16, 8, 8);
  for (uint32_t y = 0; y < 16; ++y) {
    for (uint32_t x = 0; x < 16; ++x) {
      const double dx = (x + 0.5 - 8) / 8, dy = (y + 0.5 - 8) / 8;
      const double ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
      const double a = (1 - ax) * (1 - ay) * (ax < 0.12 || ay < 0.12 ? 1.0 : 0.25);
      if (a <= 0) continue;
      put(f, x, y, 1, 0.9, 0.6, a * 1.6 > 1 ? 1 : a * 1.6);
    }
  }
  return f;
}

// The frames' indices.
constexpr uint32_t kBush = 0, kSign = 1, kBubble = 2, kSpark = 3, kFlame0 = 4, kFlames = 6,
                   kFire0 = kFlame0 + kFlames, kFires = 8;

std::vector<gr::SpriteFrame> all_frames() {
  std::vector<gr::SpriteFrame> v = {bush(), sign(), bubble(), spark()};
  for (uint32_t k = 0; k < kFlames; ++k) v.push_back(flame(k));
  for (uint32_t k = 0; k < kFires; ++k) v.push_back(fireball(k));
  return v;
}

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
  gr::Sprites sprites;
  gr::Lights lights;  // the lamps a view sets (`lamps`), none by default
  std::vector<gr::LocalLight> lamps;
  gr::Post post;
  gr::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

bool make_scene(ge::gpu::Device& d, Scene& s, uint32_t w, uint32_t h,
                const std::vector<gr::SpriteFrame>& frames) {
  s.w = w;
  s.h = h;
  s.field = ge::core::create_heightfield(-64, -64, 1, 129, 129);
  s.albedo.resize(size_t{129} * 129 * 4);
  for (int r = 0; r < 129; ++r) {
    for (int c = 0; c < 129; ++c) {
      const size_t i = static_cast<size_t>(r) * 129 + static_cast<size_t>(c);
      const int m = ((c / 2 + r / 2) & 1) * 10 + ((c * 7 + r * 13) % 7) - 3;
      const uint8_t px[4] = {static_cast<uint8_t>(118 + m), static_cast<uint8_t>(112 + m),
                             static_cast<uint8_t>(96 + m), 255};
      std::memcpy(&s.albedo[i * 4], px, 4);
    }
  }
  s.targets = gr::create_targets(d, "gpu-sprites", w, h);
  s.uniforms = gr::make_buffer(d, "gpu-sprites.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  s.layout = gr::frame_layout(d);
  gr::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  gr::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.base_spacing = 1.0;
  src.levels = 6;
  gr::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  if (!gr::create_sprites(d, s.pipelines, s.sprites, s.layout, frames)) return false;
  gr::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  if (!s.pipelines.finish(d)) return false;
  ge::gpu::end_load(d);
  gr::create_lights(d, s.lights);
  s.frame = gr::frame_group(d, s.layout, s.uniforms, s.sky, s.lights.buffer);
  s.post_group = gr::post_group(d, s.post, s.targets.hdr_view);
  s.rec.init(d, "gpu-sprites.timer", 8);
  s.out = ge::gpu::create_target(d, "gpu-sprites.final", w, h);
  return true;
}

struct Timed {
  gl::Picture pic;
  double ms = 0;  // the sprites pass on the GPU, 0 when not measured
};

Timed draw(ge::gpu::Device& d, Scene& s, const gr::FrameView& v,
           const std::vector<gr::SpriteDraw>& draws) {
  s.rec.begin_frame(d);
  gr::fill_uniforms(s.staged, v, s.w, s.h);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  gr::prepare_sky(s.sky, v);
  gr::prepare_terrain(d, s.terrain, v);
  gr::prepare_sprites(d, s.sprites, draws, v);
  gr::prepare_lights(d, s.lights, s.lamps, v, s.w, s.h);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  gr::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  gr::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  gr::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_sprites(s.sprites, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, s.out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, s.out);
  s.rec.wait(d);
  Timed out{gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba}, 0};
  if (s.rec.timed) {
    for (int i = 0; i < s.rec.stats.count; ++i) {
      const gr::PassStat& p = s.rec.stats.passes[static_cast<size_t>(i)];
      if (p.name == "sprites") out.ms = p.gpu_ms;
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
  v.fov_y = 0.8;
  v.sun_elevation = sun_el;
  v.sun_azimuth = 2.4;
  v.altitude_m = 300;
  return v;
}

gr::SpriteDraw at(uint32_t frame, double x, double y, double z, gr::SpriteFacing facing,
                  gr::SpriteBlend blend, float scale) {
  gr::SpriteDraw d;
  d.frame = frame;
  d.position[0] = d.last[0] = x;
  d.position[1] = d.last[1] = y;
  d.position[2] = d.last[2] = z;
  d.facing = facing;
  d.blend = blend;
  d.scale = scale;
  return d;
}

// The yard: two bushes, a sign turned a little toward the eye, two torches with flames, a
// fireball, bubbles rising and a spark, at time `t` (s) and the frames' rate.
std::vector<gr::SpriteDraw> yard(double t, bool smooth = true) {
  using F = gr::SpriteFacing;
  using B = gr::SpriteBlend;
  std::vector<gr::SpriteDraw> v;
  v.push_back(at(kBush, -2.2, 0, 6, F::parallel_upright, B::cutout, 0.045f));
  v.push_back(at(kBush, 2.6, 0, 7.5, F::parallel_upright, B::cutout, 0.05f));
  gr::SpriteDraw s = at(kSign, 0.9, 0, 4.5, F::oriented, B::cutout, 0.035f);
  s.rotation[1] = static_cast<float>(libm::sin(-0.25));  // turned half a radian
  s.rotation[3] = static_cast<float>(libm::cos(-0.25));
  v.push_back(s);
  for (int i = 0; i < 2; ++i) {
    const double x = i == 0 ? -1.0 : 1.9;
    const auto k = static_cast<uint32_t>(static_cast<int64_t>(t * 10 + i * 3) % kFlames);
    gr::SpriteDraw f = at(kFlame0 + k, x, 1.0, 5.5, F::upright, B::add, 0.022f);
    f.glow = 2.5f;
    f.outdoor = 0;
    v.push_back(f);
  }
  gr::SpriteDraw ball = at(kFire0 + 2, -0.4, 2.2, 8, F::parallel, B::add, 0.06f);
  ball.glow = 3;
  ball.outdoor = 0;
  v.push_back(ball);
  for (int i = 0; i < 6; ++i) {
    v.push_back(at(kBubble, -1.5 + 0.18 * i, 0.4 + 0.25 * i, 4.2, F::parallel, B::over, 0.012f));
  }
  gr::SpriteDraw sp = at(kSpark, 1.4, 1.9, 5, F::parallel_oriented, B::add, 0.02f);
  sp.roll = static_cast<float>(0.4 + t);
  sp.glow = 3;
  sp.outdoor = 0;
  v.push_back(sp);
  for (gr::SpriteDraw& d : v) d.smooth = smooth;
  return v;
}

// The mean colour of a picture's pixels inside a box.
double mean_luma(const gl::Picture& p, int x0, int y0, int x1, int y1) {
  double sum = 0;
  int n = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(p.width) + x) * 4;
      sum += 0.2126 * p.rgba[i] + 0.7152 * p.rgba[i + 1] + 0.0722 * p.rgba[i + 2];
      ++n;
    }
  }
  return n > 0 ? sum / n : 0;
}

void run(gl::Lab& lab) {
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = static_cast<uint32_t>(lab.arg_num("height"));
  const std::vector<gr::SpriteFrame> frames = all_frames();
  lab.metric("frames", static_cast<double>(frames.size()), "count");
  auto device = ge::gpu::create_device({.label = "gpu-sprites", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(
        lab,
        {"yard", "from-above", "sharp-smooth", "fireball", "dusk", "lamp-night-off", "lamp-night"},
        w, h);
    return;
  }
  Scene s;
  if (!make_scene(*device, s, w, h, frames)) {
    lab.fail("the sprites pass's pipelines did not build or its frames did not fit",
             "Read the gpu: lines above. Skill: render.");
    return;
  }

  const gr::FrameView day_eye = eye(0, 1.4, 0, 0, -0.02, 0.7);
  const gl::Picture day = draw(*device, s, day_eye, yard(0.3)).pic;
  lab.shot({"yard",
            "EVERY KIND OF SPRITE BY DAY",
            "Two bushes cut out and standing upright with the camera, a sign cut out and fixed "
            "in the world, two torch flames and a fireball of added light, bubbles laid over and "
            "a spark rolled in the screen's plane.",
            "the ground showing through the bushes' holes with clean edges; the flames and the "
            "fireball glowing; the bubbles faint rings with a glint; the sign's arrow sharp",
            {{"view", "day, eye 1.4 m"}}},
           day, Det::measured);

  {
    // The same yard from high above, looking down: the rules show.
    std::vector<gl::Picture> pics = {day,
                                     draw(*device, s, eye(0, 7, -2, 0, -0.85, 0.7), yard(0.3)).pic};
    lab.shot({"from-above",
              "THE FACING RULES, LOW AND HIGH",
              "The yard from the eye's height (left) and from 7 m looking steeply down (right).",
              "on the right the bushes and the flames still stand upright (foreshortened to "
              "slivers seen from above), the fireball and the bubbles still face the camera "
              "full on, the sign keeps its own turn in the world",
              {{"views", "1.4 m level; 7 m, 49 degrees down"}}},
             es::sheet_of(pics, {"LEVEL", "FROM ABOVE"}, 2), Det::measured);
  }

  {
    std::vector<gl::Picture> pics;
    const gr::FrameView close = eye(0.6, 0.45, 3.4, 0.2, 0.05, 0.7);
    pics.push_back(draw(*device, s, close, yard(0.3, false)).pic);
    pics.push_back(draw(*device, s, close, yard(0.3, true)).pic);
    lab.shot({"sharp-smooth",
              "SHARP (LEFT) AND SMOOTH (RIGHT)",
              "The sign and a flame close, their texels read sharp (each a square: the classic "
              "look) and smooth (bilinear over premultiplied texels).",
              "blocky texels on the left; soft edges on the right with no dark fringe round the "
              "arrow or the flame",
              {{"view", "1 m from the sign"}}},
             es::sheet_of(pics, {"SHARP", "SMOOTH"}, 2), Det::measured);
  }

  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    for (uint32_t k = 0; k < kFires; ++k) {
      gr::SpriteDraw ball =
          at(kFire0 + k, 0, 1.2, 3, gr::SpriteFacing::parallel, gr::SpriteBlend::add, 0.05f);
      ball.glow = 3;
      ball.outdoor = 0;
      pics.push_back(draw(*device, s, eye(0, 1.2, 0, 0, 0, -0.06), {ball}).pic);
      names.push_back("FRAME " + std::to_string(k));
    }
    lab.shot({"fireball",
              "THE FIREBALL'S FRAMES AT DUSK",
              "The eight frames of the fireball, added light in the screen's plane.",
              "a bright ball swelling, then cooling, dimming and thinning away to nothing",
              {{"frames", "8"}}},
             es::sheet_of(pics, names, 4), Det::measured);
  }

  const gl::Picture dusk = draw(*device, s, eye(0, 1.4, 0, 0, -0.02, -0.07), yard(0.3)).pic;
  lab.shot({"dusk",
            "THE YARD AT DUSK",
            "The yard after sunset: the bushes and the sign take the twilight's light, the "
            "flames, the fireball and the spark their own.",
            "dim bushes and sign; the flames, the fireball and the spark as bright as by day",
            {{"view", "dusk, eye 1.4 m"}}},
           dusk, Det::measured);
  // The fireball (glowing) against the near bush (lit) at dusk and by day, each sampled at the
  // pixel its centre projects to.
  auto box = [&](const gl::Picture& p, double x, double y, double z) {
    gr::FrameView v = eye(0, 1.4, 0, 0, -0.02, 0.7);
    const gr::CameraMatrices m = gr::camera_matrices(v, static_cast<double>(w) / h);
    const float rel[4] = {static_cast<float>(x), static_cast<float>(y - 1.4), static_cast<float>(z),
                          1};
    float clip[4];
    gr::transform(m.view_proj, rel, clip);
    const int px = static_cast<int>((clip[0] / clip[3] * 0.5 + 0.5) * w);
    const int py = static_cast<int>((0.5 - clip[1] / clip[3] * 0.5) * h);
    return mean_luma(p, px - 1, py - 1, px + 2, py + 2);
  };
  const double glow_dusk = box(dusk, -0.4, 2.2, 8), lit_dusk = box(dusk, -2.2, 0.6, 6);
  lab.metric("dusk.fireball", glow_dusk, "level", Det::measured);
  lab.metric("dusk.bush", lit_dusk, "level", Det::measured);
  lab.metric("day.bush", box(day, -2.2, 0.6, 6), "level", Det::measured);
  if (glow_dusk <= lit_dusk) {
    lab.fail("at dusk the glowing fireball is no brighter than the lit bush",
             "Check fs_blend's glow term in engine/render/sprites.wgsl. Skill: render.");
  }

  {
    // THE LAMP AT NIGHT (render/lights.hpp on the cards): the yard on a moonless night under a
    // lamp of 400 cd on a 4 m post between the bushes, the eye adapted to a fiftieth of the light
    // under it; drawn with the lamp off and on.
    gr::FrameView night = eye(0, 1.4, 0, 0, -0.02, -0.35);
    gr::Moon none{-0.8, 1.0, 0.0, 384400};
    gr::set_night(night, none, 46, 120);
    gr::LocalLight lamp;
    lamp.position[0] = 0.2;
    lamp.position[1] = 4;
    lamp.position[2] = 6;
    lamp.colour[0] = 1.0f;
    lamp.colour[1] = 0.86f;
    lamp.colour[2] = 0.66f;
    lamp.candela = 400;
    lamp.inner_deg = 40;
    lamp.outer_deg = 72;
    lamp.range_m = 12;
    lamp.radius_m = 0.1f;
    const double under[3] = {0.2, 0, 6};
    night.lamp_lux = 0.02 * gr::lamp_lux_at(std::span(&lamp, 1), under, 0);
    s.lamps.clear();
    const gl::Picture off = draw(*device, s, night, yard(0.3)).pic;
    s.lamps.push_back(lamp);
    const gl::Picture on = draw(*device, s, night, yard(0.3)).pic;
    const gl::Picture ground = draw(*device, s, night, {}).pic;
    s.lamps.clear();
    auto at_bush = [&](const gl::Picture& p) {
      const gr::CameraMatrices m = gr::camera_matrices(night, static_cast<double>(w) / h);
      const float rel[4] = {-2.2f, static_cast<float>(0.6 - 1.4), 6, 1};
      float clip[4];
      gr::transform(m.view_proj, rel, clip);
      const int px = static_cast<int>((clip[0] / clip[3] * 0.5 + 0.5) * w);
      const int py = static_cast<int>((0.5 - clip[1] / clip[3] * 0.5) * h);
      return mean_luma(p, px - 1, py - 1, px + 2, py + 2);
    };
    const double bush_off = at_bush(off), bush_on = at_bush(on);
    lab.metric("lamp-night.bush-off", bush_off, "level", Det::measured);
    lab.metric("lamp-night.bush-on", bush_on, "level", Det::measured);
    lab.metric("lamp-night.ground-under", at_bush(ground), "level", Det::measured);
    if (bush_on <= bush_off + 4) {
      lab.fail("the lamp lights none of the near bush",
               "Check sprites.wgsl's lamps_on_card and that the scene's frame_group binds "
               "Lights::buffer. Skill: render.");
    }
    const std::string caption =
        "The yard on a moonless night, a lamp of 400 cd on a 4 m post between the bushes, its "
        "cone straight down; the eye adapted to a fiftieth of the light under it.";
    lab.shot({"lamp-night-off",
              "THE YARD AT NIGHT, THE LAMP OFF",
              caption,
              "the bushes and the sign black against the dark ground; only the flames, the "
              "fireball and the spark bright",
              {}},
             off, Det::measured);
    lab.shot({"lamp-night",
              "THE YARD AT NIGHT UNDER A LAMP",
              caption,
              "the bushes and the sign lit warm like the ground in the lamp's pool, brightest "
              "nearest the post",
              {{"lamp", "400 cd"}}},
             on, Det::measured);
  }

  {
    // The cost: 2000 sprites, a quarter of them blended.
    std::vector<gr::SpriteDraw> crowd;
    crowd.reserve(2000);
    for (int i = 0; i < 2000; ++i) {
      const double x = -20 + 40 * hash01(static_cast<uint32_t>(i), 1, 9);
      const double z = 3 + 40 * hash01(static_cast<uint32_t>(i), 2, 9);
      const bool blended = i % 4 == 0;
      gr::SpriteDraw d =
          at(blended ? kFire0 + static_cast<uint32_t>(i) % kFires : kBush, x, blended ? 1.5 : 0, z,
             blended ? gr::SpriteFacing::parallel : gr::SpriteFacing::parallel_upright,
             blended ? gr::SpriteBlend::add : gr::SpriteBlend::cutout, 0.04f);
      d.glow = blended ? 2.0f : 0.0f;
      crowd.push_back(d);
    }
    double ms = 0;
    for (int k = 0; k < 3; ++k) ms = draw(*device, s, eye(0, 2, 0, 0, -0.05, 0.7), crowd).ms;
    lab.metric("crowd.sprites-gpu-ms", ms, "ms", Det::measured);
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
             "Every pipeline belongs in create_sprites. Skill: render.");
  }
}

const gl::LabSpec kSpec{
    "gpu-sprites",
    gl::LabKind::picture,
    "engine",
    "engine/render/sprites.hpp",
    "render",
    "Do sprites turn by their facing rules, cut out cleanly or blend over the picture, read sharp "
    "or smooth, glow where they should and take the sky's light where they should not?",
    {
        {"width", "640", "each view's width, px"},
        {"height", "360", "each view's height, px"},
    },
    "--width=160 --height=90",
    run,
};
GE_LAB(kSpec);

}  // namespace
