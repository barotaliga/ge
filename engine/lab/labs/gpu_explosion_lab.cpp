// LAB `gpu-explosion`: EXPLOSIONS (render/explosion.hpp, render/explosions.hpp) on the software
// adapter, on a flat dirt yard with a 4 m wall across its far side. Named views: a blast on the
// ground 14 m ahead through its life by day and at dusk (its fireball swelling white-hot,
// orange, then smoke rising, its sparks streaking out and reddening); a rocket's and a grenade's
// blast through their lives side by side (one code, two looks); every preset at two moments; a
// sweep of the knobs, each at its two ends; a blast against the wall's foot (the wall and the
// ground clip the ball pixel by pixel); the eye inside a ball; the march's steps 6, 10 and 16
// beside each other; and with `--frames=N` N frames of the day sequence for an animation.
// `--look=<id>` picks the preset the day, dusk and animation views draw and `--set=knob=value,...`
// turns its knobs (render/explosion.hpp's `blast_knobs`). Measured: what a blast costs the GPU, as
// the wall clock of a frame with one, with 32 and with one filling the screen, against the same
// frame without (the software adapter: only the ratios mean anything; a real GPU's times need the
// Mac), the pixels a blast's march runs on, and the CPU's staging of 32 blasts. It fails on a GPU
// error or a pipeline made after the load. Without an adapter it notes that, with grey cards.
//
// ge:summary Lab: explosions through their life, looks and knobs, clipping, steps, and their cost
// ge:keywords lab gpu explosion fireball smoke sparks blast cost steps clip sequence animation
// ge:keywords knobs preset rocket grenade
// ge:keywords picture software-adapter
// ge:skill render labs
// ge:test tests/render_explosion_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/raster.hpp"
#include "render/explosion.hpp"
#include "render/explosions.hpp"
#include "render/frame.hpp"
#include "render/particles.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::paste;
using ge::lab::water_scene::placeholders;
using gl::Det;

// THE YARD: 256 m of flat dirt on 1 m cells, a wall 4 m high across it at z = 140.
constexpr double kMid = 128, kWallZ = 140, kWallH = 4;

float yard(double, double z) { return z >= kWallZ ? static_cast<float>(kWallH) : 0.0f; }

struct Scene {
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms;
  gr::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group;
  gr::Sky sky;
  gr::Terrain terrain;
  gr::Particles particles;
  gr::Explosions explosions;
  gr::Post post;
  gr::FrameRecorder rec;
  ge::core::Heightfield field;
  std::vector<uint8_t> albedo;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

bool make_scene(ge::gpu::Device& d, Scene& s, uint32_t w, uint32_t h, uint32_t steps) {
  s.w = w;
  s.h = h;
  s.field = ge::core::create_heightfield(0, 0, 1, 257, 257);
  s.albedo.resize(size_t{257} * 257 * 4);
  for (int r = 0; r < 257; ++r) {
    for (int c = 0; c < 257; ++c) {
      const size_t i = static_cast<size_t>(r) * 257 + static_cast<size_t>(c);
      s.field.data[i] = yard(c, r);
      // Packed dirt, a little mottled; the wall a pale stone.
      const bool wall = r >= kWallZ - 1;
      const int m = ((c * 7 + r * 13) % 9) - 4;
      const uint8_t px[4] = {static_cast<uint8_t>((wall ? 168 : 118) + m),
                             static_cast<uint8_t>((wall ? 160 : 98) + m),
                             static_cast<uint8_t>((wall ? 146 : 76) + m), 255};
      std::memcpy(&s.albedo[i * 4], px, 4);
    }
  }
  s.targets = gr::create_targets(d, "gpu-explosion", w, h);
  s.uniforms = gr::make_buffer(d, "gpu-explosion.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  s.layout = gr::frame_layout(d);
  gr::create_sky(d, s.pipelines, s.sky, s.uniforms, s.layout);
  gr::TerrainSource src;
  src.height = &s.field;
  src.albedo = &s.albedo;
  src.base_spacing = 1.0;
  src.levels = 7;
  gr::create_terrain(d, s.pipelines, s.terrain, src, s.layout);
  gr::create_particles(d, s.pipelines, s.particles, s.layout, s.terrain, {.capacity = 4096});
  gr::create_explosions(d, s.pipelines, s.explosions, s.layout, {.steps = steps});
  gr::create_post(d, s.pipelines, s.post, s.layout, wgpu::TextureFormat::RGBA8Unorm);
  if (!s.pipelines.finish(d)) return false;
  ge::gpu::end_load(d);
  s.frame = gr::frame_group(d, s.layout, s.uniforms, s.sky);
  gr::bind_particle_targets(d, s.particles, s.targets);
  gr::bind_explosion_targets(d, s.explosions, s.targets);
  s.post_group = gr::post_group(d, s.post, s.targets.hdr_view);
  s.rec.init(d, "gpu-explosion.timer", 10);
  s.out = ge::gpu::create_target(d, "gpu-explosion.final", w, h);
  return true;
}

gr::FrameView eye_at(double x, double y, double z, double pitch, double sun_el, double t,
                     uint64_t frame) {
  gr::FrameView v;
  v.eye[0] = x;
  v.eye[1] = y;
  v.eye[2] = z;
  v.pitch = pitch;
  v.fov_y = 1.0;
  v.sun_elevation = sun_el;
  v.sun_azimuth = 2.3;
  v.altitude_m = 300;
  v.time_s = t;
  v.frame = frame;
  return v;
}

struct Timed {
  gl::Picture pic;
  double ms = 0;        // the explosions pass on the GPU (its timestamps), else the frame's wall
  double frame_ms = 0;  // every pass on the GPU
};

// One frame: the particles flown `dt` s first (births waiting go in), then the picture.
Timed draw(ge::gpu::Device& d, Scene& s, const gr::FrameView& v,
           const std::vector<gr::Blast>& blasts, double dt) {
  s.rec.begin_frame(d);
  gr::fill_uniforms(s.staged, v, s.w, s.h);
  d.queue.WriteBuffer(s.uniforms, 0, &s.staged, sizeof s.staged);
  gr::prepare_sky(s.sky, v);
  gr::prepare_terrain(d, s.terrain, v);
  gr::prepare_particles(d, s.particles, dt, v.near_m);
  gr::prepare_explosions(d, s.explosions, blasts, v, s.w, s.h);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(s.sky, s.pipelines, s.rec, e);
  gr::encode_particles_sim(s.particles, s.pipelines, s.rec, e);
  gr::encode_terrain_cull(s.terrain, s.pipelines, s.rec, e, s.frame);
  gr::encode_terrain_horizon(s.terrain, s.pipelines, s.rec, e);
  gr::encode_terrain_depth(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_terrain(s.terrain, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_sky(s.sky, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_particles(s.particles, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_explosions(s.explosions, s.pipelines, s.rec, e, s.targets, s.frame);
  gr::encode_post(s.post, s.pipelines, s.rec, e, s.frame, s.post_group, s.out.view);
  s.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  const auto t0 = std::chrono::steady_clock::now();
  d.queue.Submit(1, &c);
  s.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, s.out);
  const auto t1 = std::chrono::steady_clock::now();
  s.rec.wait(d);
  Timed out{gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
            std::chrono::duration<double, std::milli>(t1 - t0).count(), 0};
  out.frame_ms = out.ms;
  if (s.rec.timed) {
    out.frame_ms = 0;
    for (int i = 0; i < s.rec.stats.count; ++i) {
      const gr::PassStat& p = s.rec.stats.passes[static_cast<size_t>(i)];
      if (p.gpu_ms == p.gpu_ms) out.frame_ms += p.gpu_ms;  // NaN: not measured
      if (p.name == "explosions") out.ms = p.gpu_ms;
    }
  }
  return out;
}

// The march's jitter averaged over `n` frames at one moment, as the TAA resolve settles a still.
gl::Picture settled(ge::gpu::Device& d, Scene& s, gr::FrameView v,
                    const std::vector<gr::Blast>& blasts, int n) {
  std::vector<uint32_t> sum;
  gl::Picture first;
  for (int k = 0; k < n; ++k) {
    v.frame = static_cast<uint64_t>(k);
    const gl::Picture p = draw(d, s, v, blasts, 0).pic;
    if (k == 0) {
      first = p;
      sum.assign(p.rgba.size(), 0);
    }
    for (size_t i = 0; i < p.rgba.size(); ++i) sum[i] += p.rgba[i];
  }
  for (size_t i = 0; i < sum.size(); ++i) first.rgba[i] = static_cast<uint8_t>(sum[i] / n);
  return first;
}

gr::Blast blast_at(double x, double y, double z, uint32_t seed, const gr::BlastLook* look = nullptr,
                   double born = 0) {
  gr::Blast b;
  b.at[0] = x;
  b.at[1] = y;
  b.at[2] = z;
  b.on_surface = true;
  b.seed = seed;
  b.look = look;
  b.born_s = born;
  return b;
}

// A life's sequence from one eye: the blast goes off at 0, its sparks thrown then, the pool
// flown frame to frame between the moments shown.
std::vector<gl::Picture> sequence(ge::gpu::Device& d, Scene& s, const gr::Blast& b,
                                  const std::vector<double>& moments, double sun_el, int settle) {
  gr::clear_particles(d, s.particles);
  gr::throw_blast(s.particles, b);
  const std::vector<gr::Blast> one = {b};
  std::vector<gl::Picture> out;
  out.reserve(moments.size());
  double t = 0;
  for (const double m : moments) {
    while (t + 1e-9 < m) {
      const double dt = std::min(1.0 / 60, m - t);
      t += dt;
      draw(d, s, eye_at(kMid, 1.7, 100, -0.02, sun_el, t, 0), one, dt);
    }
    out.push_back(settled(d, s, eye_at(kMid, 1.7, 100, -0.02, sun_el, m, 0), one, settle));
  }
  return out;
}

gl::Picture sheet_of(const std::vector<gl::Picture>& pics, const std::vector<std::string>& labels,
                     int cols) {
  const int tw = pics[0].width, th = pics[0].height;
  const int rows = (static_cast<int>(pics.size()) + cols - 1) / cols;
  gl::Picture sheet =
      gl::create_picture(cols * (tw + 8) + 8, rows * (th + 16) + 6, {250, 250, 248});
  for (size_t i = 0; i < pics.size(); ++i) {
    const int ox = 8 + static_cast<int>(i % static_cast<size_t>(cols)) * (tw + 8);
    const int oy = 4 + static_cast<int>(i / static_cast<size_t>(cols)) * (th + 16);
    gl::draw_text(sheet, ox, oy, labels[i], {24, 26, 32});
    paste(sheet, pics[i], ox, oy + 11);
  }
  return sheet;
}

// Pixels that differ between two pictures by more than a few levels.
int changed(const gl::Picture& a, const gl::Picture& b) {
  int n = 0;
  for (size_t i = 0; i + 3 < a.rgba.size(); i += 4) {
    int dl = 0;
    for (int c = 0; c < 3; ++c) dl = std::max(dl, std::abs(a.rgba[i + c] - b.rgba[i + c]));
    n += dl > 3 ? 1 : 0;
  }
  return n;
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const auto h = static_cast<uint32_t>(lab.arg_num("height"));
  const int settle = std::clamp(static_cast<int>(lab.arg_num("settle")), 1, 16);
  const int frames = std::clamp(static_cast<int>(lab.arg_num("frames")), 0, 600);
  if (w < 64 || h < 36 || w > 4096 || h > 4096) {
    lab.fail("--width must be 64..4096 and --height 36..4096", "next: pass --width=640.");
    return;
  }
  // The look the day, dusk and animation views draw: a preset, its knobs turned.
  const gr::BlastLook* preset = gr::blast_look_by_id(lab.arg("look"));
  if (!preset) {
    lab.fail("no blast look \"" + lab.arg("look") + "\"",
             "next: pass --look=explosion (or .rocket, .grenade, .small, .large).");
    return;
  }
  gr::BlastLook chosen = *preset;
  for (std::string_view rest = lab.arg("set"); !rest.empty();) {
    const size_t comma = rest.find(',');
    const std::string_view one = rest.substr(0, comma);
    rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
    const size_t eq = one.find('=');
    if (eq == std::string_view::npos ||
        !gr::set_knob(chosen, one.substr(0, eq),
                      std::strtod(std::string(one.substr(eq + 1)).c_str(), nullptr))) {
      lab.fail(
          "--set: \"" + std::string(one) + "\" is not knob=value for a known knob",
          "next: name a knob from render/explosion.hpp's blast_knobs, e.g. --set=fire_time=1.");
      return;
    }
  }
  // The CPU's half: 32 blasts staged, timed over many frames.
  {
    gr::Explosions ex;
    ex.staged.assign(gr::kMaxExplosions, gr::GpuBlast{});
    ex.order.reserve(256);
    std::vector<gr::Blast> many;
    many.reserve(32);
    for (uint32_t i = 0; i < 32; ++i) {
      many.push_back(blast_at(kMid - 24 + 1.5 * i, 0, 112 + (i % 4) * 5.0, i, nullptr, -0.05 * i));
    }
    const auto t0 = std::chrono::steady_clock::now();
    constexpr int kRounds = 2000;
    for (int k = 0; k < kRounds; ++k) {
      gr::stage_explosions(ex, many, eye_at(kMid, 1.7, 100, -0.02, 0.6, 1.0, 0), w, h);
    }
    const double us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() /
        kRounds;
    lab.metric("cpu.stage-32", us, "us", Det::measured);
  }
  auto device = ge::gpu::create_device({.label = "gpu-explosion", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(
        lab,
        {"day", "dusk", "rocket-grenade", "presets", "knobs", "wall", "inside", "field", "steps"},
        w, h);
    return;
  }
  Scene s;
  if (!make_scene(*device, s, w, h, 10)) {
    lab.fail("the explosions' pipelines did not build",
             "Read the gpu: lines above. Skill: render.");
    return;
  }
  const gr::Blast centre = blast_at(kMid, 0, 110, 7);
  const gr::Blast shown = blast_at(kMid, 0, 110, 7, &chosen);
  const std::vector<double> moments = {0.02, 0.06, 0.12, 0.25, 0.45, 0.8, 1.3, 2.1, 3.2};
  std::vector<std::string> labels;
  labels.reserve(moments.size());
  for (const double m : moments) labels.push_back(fixed(m, 2) + " S");

  lab.shot({"day",
            "A BLAST THROUGH ITS LIFE, BY DAY",
            "The common blast (\"explosion\") on the dirt 10 m ahead, under a sun 34 degrees up, "
            "at nine moments of its life; the march's jitter settled over " +
                std::to_string(settle) + " frames as the resolve settles it.",
            "a white-hot core swelling in a twentieth of a second, sparks streaking out; the "
            "ball orange then dull red as it cools, its edge rolling in billows; grey smoke "
            "lit on its sunward side rising and thinning away; nothing left at the end",
            {{"look", std::string(chosen.id)}}},
           sheet_of(sequence(*device, s, shown, moments, 0.6, settle), labels, 3), Det::measured);
  lab.shot({"dusk",
            "THE SAME BLAST AT DUSK",
            "As above, under a sun 3 degrees below the horizon: the eye adapted to the dusk, "
            "so the fire is far brighter than its surroundings.",
            "the fireball blown white-yellow and glowing long, the sparks bright red streaks, "
            "the smoke dark against the sky, the embers glowing a dull red in it",
            {{"look", std::string(chosen.id)}}},
           sheet_of(sequence(*device, s, shown, moments, -0.05, settle), labels, 3), Det::measured);

  // A rocket's and a grenade's blast: one code, two looks, through their lives.
  {
    const std::vector<double> at = {0.04, 0.15, 0.4, 1.0, 2.2};
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    pics.reserve(2 * at.size());
    names.reserve(2 * at.size());
    for (const char* id : {"explosion.rocket", "explosion.grenade"}) {
      const gr::Blast b = blast_at(kMid, 0, 110, 7, gr::blast_look_by_id(id));
      for (gl::Picture& p : sequence(*device, s, b, at, 0.6, settle)) pics.push_back(std::move(p));
      for (const double m : at)
        names.push_back(std::string(std::string_view(id).substr(10)) + " " + fixed(m, 2) + " S");
    }
    lab.shot({"rocket-grenade",
              "A ROCKET AND A GRENADE",
              "\"explosion.rocket\" (top) and \"explosion.grenade\" (bottom) 10 m ahead at five "
              "moments: the same code, two looks.",
              "the rocket white-hot and quick, a spray of fast streaking sparks, thin dark smoke "
              "rising fast; the grenade a duller, short flash, a fat cloud of pale brown dust, "
              "more chunks of debris on arcs, the cloud hanging longer",
              {}},
             sheet_of(pics, names, 5), Det::measured);
  }

  // Every preset at a fifth and at 1.2 s.
  {
    const auto looks = gr::blast_looks();
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    pics.reserve(2 * looks.size());
    names.reserve(2 * looks.size());
    for (const double m : {0.2, 1.2}) {
      for (size_t k = 0; k < looks.size(); ++k) {
        const gr::Blast b = blast_at(kMid, 0, 116, 11 + static_cast<uint32_t>(k), &looks[k]);
        gr::clear_particles(*device, s.particles);
        pics.push_back(settled(*device, s, eye_at(kMid, 1.7, 100, 0.02, 0.6, m, 0), {b}, settle));
        names.push_back(std::string(looks[k].id) + " " + fixed(m, 1) + " S");
      }
    }
    lab.shot({"presets",
              "THE PRESETS",
              "Every preset 16 m ahead, at 0.2 s and at 1.2 s, fire and smoke only.",
              "five looks of the same fire: sizes, heats and smokes that tell them apart",
              {}},
             sheet_of(pics, names, static_cast<int>(looks.size())), Det::measured);
  }

  // The knobs, each at its two ends, on the common blast at 0.5 s.
  {
    struct Sweep {
      const char* knob;
      double lo, hi;
      double at;
    };
    const Sweep sweeps[] = {{"fire", 0.25, 1, 0.15},        {"fire_time", 0.2, 1.4, 0.6},
                            {"heat", 1600, 3000, 0.08},     {"smoke", 0.3, 1.8, 1},
                            {"smoke_albedo", 0.05, 0.6, 1}, {"billows", 0.15, 1.4, 0.5},
                            {"billow_size", 0.5, 2.2, 0.5}, {"fray", 0, 0.8, 0.5}};
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    pics.reserve(2 * std::size(sweeps));
    names.reserve(2 * std::size(sweeps));
    for (const Sweep& sw : sweeps) {
      for (const double v : {sw.lo, sw.hi}) {
        gr::BlastLook look = gr::default_look();
        gr::set_knob(look, sw.knob, v);
        const gr::Blast b = blast_at(kMid, 0, 112, 7, &look);
        gr::clear_particles(*device, s.particles);
        pics.push_back(
            settled(*device, s, eye_at(kMid, 1.7, 100, 0.03, 0.6, sw.at, 0), {b}, settle));
        names.push_back(std::string(sw.knob) + "=" + fixed(v, 2) + " " + fixed(sw.at, 2) + " S");
      }
    }
    lab.shot({"knobs",
              "THE KNOBS AT THEIR ENDS",
              "The common blast 12 m ahead with one knob turned low (left) and high (right), each "
              "at the moment it shows best.",
              "each pair differing only in what the knob names: the fire's amount, its duration "
              "and heat, the smoke's thickness and paleness, the billows' depth, size and fray",
              {}},
             sheet_of(pics, names, 4), Det::measured);
  }

  // Against the wall's foot: clipped by the wall and the ground.
  {
    gr::Blast b = blast_at(kMid, 0.3, kWallZ - 1.0, 21);
    b.normal[1] = 0;
    b.normal[2] = -1;  // off the wall's face, toward the eye
    std::vector<gl::Picture> pics;
    pics.reserve(2);
    for (const double m : {0.1, 0.6}) {
      pics.push_back(
          settled(*device, s, eye_at(kMid - 6, 2.5, 126, -0.05, 0.6, m, 0), {b}, settle));
    }
    lab.shot({"wall",
              "A BLAST AT THE WALL'S FOOT",
              "The common blast going off against the 4 m wall from 15 m off, at 0.1 and 0.6 s: "
              "the ball is cut by the scene's depth, not faded against it.",
              "a half ball hugging the wall and the ground, its edges crisp where it meets them, "
              "no fire drawn inside the wall or under the floor",
              {}},
             sheet_of(pics, {"0.10 S", "0.60 S"}, 2), Det::measured);
  }

  // THE COST: the explosions pass's GPU time (its timestamps) with none, one, 32 and one
  // filling the screen (the eye inside it), each the median of seven.
  auto median_ms = [&](const gr::FrameView& v, const std::vector<gr::Blast>& bl) {
    std::vector<double> t;
    t.reserve(7);
    gr::clear_particles(*device, s.particles);
    for (int k = 0; k < 3; ++k) draw(*device, s, v, bl, 0);
    for (int k = 0; k < 7; ++k) t.push_back(draw(*device, s, v, bl, 0).ms);
    std::sort(t.begin(), t.end());
    return t[3];
  };
  const gr::FrameView far_eye = eye_at(kMid, 1.7, 100, -0.02, 0.6, 0.3, 0);
  const double base = median_ms(far_eye, {});
  const double one = median_ms(far_eye, {centre});
  std::vector<gr::Blast> many;
  many.reserve(32);
  for (uint32_t i = 0; i < 32; ++i) {
    many.push_back(
        blast_at(kMid - 24 + 1.5 * i, 0, 112 + (i % 4) * 5.0, i, nullptr, 0.3 - 0.07 * i));
  }
  const double field = median_ms(far_eye, many);
  const uint32_t drawn32 = s.explosions.drawn;
  const gr::FrameView in_eye = eye_at(kMid, 1.0, 109.5, -0.02, 0.6, 0.3, 0);
  const double in_base = median_ms(in_eye, {});
  const double inside = median_ms(in_eye, {centre});
  lab.metric("timed", s.rec.timed ? 1 : 0, "bool", Det::measured);
  lab.metric("frame.gpu", draw(*device, s, far_eye, {}, 0).frame_ms, "ms", Det::measured);
  lab.metric("pass.none", base, "ms", Det::measured);
  lab.metric("pass.one", one, "ms", Det::measured);
  lab.metric("pass.32", field, "ms", Det::measured);
  lab.metric("pass.filling", inside, "ms", Det::measured);
  lab.metric("pass.filling-empty", in_base, "ms", Det::measured);
  lab.metric("drawn.32", drawn32, "count", Det::measured);
  {
    const gl::Picture bare = draw(*device, s, far_eye, {}, 0).pic;
    const gl::Picture lit = draw(*device, s, far_eye, {centre}, 0).pic;
    const int px = changed(bare, lit);
    lab.metric("pixels.one", px, "px", Det::measured);
    lab.metric("pixels.one-share", 100.0 * px / (w * h), "%", Det::measured);
    if (px > 0) lab.metric("pass.per-mpx", (one - base) / (px / 1e6), "ms", Det::measured);
  }
  lab.shot({"inside",
            "THE EYE INSIDE A BALL",
            "The eye 0.5 m from the blast's centre at 0.3 s: the whole screen is marched.",
            "the screen filled with dense grey-brown smoke lit warm from within, no seam at "
            "the screen's edge",
            {}},
           settled(*device, s, in_eye, {centre}, settle), Det::measured);
  {
    gl::Picture field_pic = settled(*device, s, far_eye, many, settle);
    lab.shot({"field",
              "THIRTY-TWO AT ONCE",
              "32 blasts in four rows 12 to 27 m ahead, each born 0.07 s before the next, seen "
              "when the youngest goes off (ages 0 to 2.2 s); the most the pass draws.",
              "fire and smoke at every stage of a blast's life, sorted: nearer balls over "
              "farther ones",
              {}},
             field_pic, Det::measured);
  }

  // THE STEPS: 6, 10 and 16 through a ball, on scenes of their own.
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> names;
    pics.reserve(3);
    names.reserve(3);
    for (const uint32_t steps : {6u, 10u, 16u}) {
      auto dev = ge::gpu::create_device({.label = "gpu-explosion.steps", .validation = 1});
      Scene q;
      if (!dev || !make_scene(*dev, q, w, h, steps)) continue;
      const gr::FrameView v = eye_at(kMid, 1.7, 104, 0.08, 0.6, 0.35, 0);
      pics.push_back(settled(*dev, q, v, {centre}, 1));
      std::vector<double> t;
      t.reserve(5);
      for (int k = 0; k < 3; ++k) draw(*dev, q, v, {centre}, 0);
      for (int k = 0; k < 5; ++k) t.push_back(draw(*dev, q, v, {centre}, 0).ms);
      std::sort(t.begin(), t.end());
      names.push_back(std::to_string(steps) + " STEPS  " + fixed(t[2], 1) + " MS");
      lab.metric("pass.steps-" + std::to_string(steps), t[2], "ms", Det::measured);
    }
    if (!pics.empty()) {
      lab.shot({"steps",
                "THE MARCH'S STEPS",
                "The same blast 6 m ahead at 0.35 s marched in 6, 10 (the default) and 16 "
                "steps, a single frame each (no settling), with the pass's GPU time.",
                "6 steps a little grainier at the billows' edges, 10 and 16 alike",
                {}},
               sheet_of(pics, names, 3), Det::measured);
    }
  }

  // The animation's frames: the day sequence at 30 a second, for a page to play.
  if (frames > 0) {
    std::vector<double> ts;
    ts.reserve(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) ts.push_back((i + 1) / 30.0);
    const std::vector<gl::Picture> pics = sequence(*device, s, shown, ts, 0.6, settle);
    for (size_t i = 0; i < pics.size(); ++i) {
      char id[32];
      std::snprintf(id, sizeof id, "anim-%03zu", i);
      lab.shot({id, "FRAME", "The day sequence at 30 frames a second.", "", {}}, pics[i],
               Det::measured);
    }
  }

  const uint32_t errors = device->errors.load();
  const uint32_t late = device->pipelines_after_load;
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0) {
    lab.fail(std::to_string(errors) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
  if (late != 0) {
    lab.fail("a pipeline was made after the load",
             "Add it to create_explosions' manifest. Skill: gpu, § Pipelines.");
  }
}

const gl::LabSpec kSpec{
    "gpu-explosion",
    gl::LabKind::picture,
    "engine",
    "engine/render/explosions.hpp",
    "render",
    "Does a blast read as fire and smoke through its life, by day and at dusk, clipped by what "
    "it meets, and how little does it cost?",
    {
        {"width", "640", "each view's width, px"},
        {"height", "360", "each view's height, px"},
        {"settle", "6", "frames the march's jitter is averaged over for a still"},
        {"frames", "0", "also N frames of the day sequence at 30 a second (an animation)"},
        {"look", "explosion", "the preset the day, dusk and animation views draw"},
        {"set", "", "knob=value,... turned on that look (render/explosion.hpp's blast_knobs)"},
    },
    "--width=128 --height=72 --settle=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
