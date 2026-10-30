// LAB `gpu-lights`: LOCAL LIGHTS AND SURFACES DRAWN (render/lights.hpp, render/surface.hpp), on
// the software adapter. A courtyard at the bottom of a moonless night (a stone wall and pillars,
// wooden crates, an iron post and a copper urn, a pool of slime and a wet flagstone floor) seen
// unlit, then lit by two torches that waver, a street lamp's spot, a failing tube and a muzzle
// flash, without their shadows and with them (render/lamp_shadows.hpp); the lamps' shadow atlas
// itself; the clusters each pixel reads its lamps from, as a heat map; the flash's first tenth of
// a second; a sheet of spheres (roughness across, dielectric, metal and the presets down) under
// the noon sun and under one lamp at night; and every flicker's level over two seconds, charted.
// It holds the binning to its CPU twin (every pixel lit by a lamp's sphere finds that lamp in its
// cluster: checked off the depth read back), the lit courtyard brighter than the unlit one where
// the lamps reach and no darker anywhere, the shadows darkening a share of it, a still frame
// redrawing no tile and a moved crate only the tiles that see it, and fails on a GPU error or a
// pipeline made after load.
//
// ge:summary Lab: local lights and surfaces drawn: a night courtyard, clusters, a flash, spheres
// ge:keywords lab gpu lights local-lights torch flame street-lamp spot muzzle-flash buzz flicker
// ge:keywords clusters heat-map materials pbr spheres roughness metal stone wood slime wet-floor
// ge:keywords lamp-shadows shadow-atlas cube-faces cached-tiles movers
// ge:skill render labs
// ge:test tests/render_lights_test.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/heightfield.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/lit_scene.hpp"
#include "lab/labs/water_scene.hpp"
#include "lab/plot.hpp"
#include "lab/raster.hpp"
#include "render/frame.hpp"
#include "render/hiz.hpp"
#include "render/instances.hpp"
#include "render/lights.hpp"
#include "render/mesh.hpp"
#include "render/night.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/surface.hpp"
#include "render/terrain.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace libm = ge::core::libm;
using ge::lab::lit_scene::at;
using ge::lab::lit_scene::draw;
using ge::lab::lit_scene::kCx;
using ge::lab::lit_scene::kCz;
using ge::lab::lit_scene::Scene;
using ge::lab::lit_scene::view_at;
using ge::lab::water_scene::fixed;
using ge::lab::water_scene::gpu_required;
using ge::lab::water_scene::paste;
using ge::lab::water_scene::placeholders;
using gl::Det;

// The props' materials, in order.
enum Mat : uint8_t {
  kStone,
  kWood,
  kIron,
  kCopper,
  kSlime,
  kWetFloor,
  kBulb,
  kFlame,
  kTube,
  kSheet
};
// The sphere sheet: kSheetCols roughnesses a row; a dielectric row, a metal row, a preset row.
constexpr int kSheetCols = 6, kSheetRows = 3;

std::vector<gr::Surface> surfaces() {
  const float stone[3] = {0.32f, 0.30f, 0.27f}, wood[3] = {0.30f, 0.17f, 0.08f};
  const float iron[3] = {0.53f, 0.51f, 0.49f}, copper[3] = {0.95f, 0.64f, 0.54f};
  const float slime[3] = {0.12f, 0.35f, 0.05f}, floor[3] = {0.10f, 0.10f, 0.11f};
  const float bulb[3] = {1.0f, 0.82f, 0.6f}, flame[3] = {1.0f, 0.55f, 0.18f};
  const float tube[3] = {0.85f, 0.95f, 1.0f};
  std::vector<gr::Surface> s = {gr::stone_surface(stone),      gr::wood_surface(wood),
                                gr::metal_surface(iron, 0.4f), gr::metal_surface(copper, 0.25f),
                                gr::slime_surface(slime),      gr::wet_floor_surface(floor),
                                gr::lamp_surface(bulb, 3e4f),  gr::lamp_surface(flame, 6e3f),
                                gr::lamp_surface(tube, 1.2e3f)};
  const float red[3] = {0.6f, 0.06f, 0.05f}, gold[3] = {1.0f, 0.77f, 0.34f};
  for (int c = 0; c < kSheetCols; ++c) {
    const float rough = 0.05f + 0.95f * static_cast<float>(c) / (kSheetCols - 1);
    gr::Surface d = gr::matte_surface(red);
    d.spec = 0.5f;
    d.rough = rough;
    s.push_back(d);
  }
  for (int c = 0; c < kSheetCols; ++c) {
    s.push_back(gr::metal_surface(gold, 0.05f + 0.95f * static_cast<float>(c) / (kSheetCols - 1)));
  }
  const float grey[3] = {0.5f, 0.5f, 0.5f};
  s.push_back(gr::matte_surface(grey));
  s.push_back(gr::stone_surface(stone));
  s.push_back(gr::wood_surface(wood));
  s.push_back(gr::metal_surface(iron, 0.4f));
  s.push_back(gr::slime_surface(slime));
  s.push_back(gr::wet_floor_surface(floor));
  return s;
}

// THE COURTYARD: the wall 14 m ahead of the eye, two pillars, crates, the street lamp's post and
// its arm, the copper urn, the slime's pool and the wet flags.
constexpr size_t kSmallCrate = 5;  // the crate the mover shot moves
std::vector<gr::Instance> courtyard() {
  std::vector<gr::Instance> v;
  v.push_back(at(kCx, 0, kCz + 14, 26, 6, 1, 0, kStone));           // the wall
  v.push_back(at(kCx - 6, 0, kCz + 10, 1.2f, 5, 1.2f, 0, kStone));  // the pillars
  v.push_back(at(kCx + 6, 0, kCz + 10, 1.2f, 5, 1.2f, 0, kStone));
  v.push_back(at(kCx - 3.2, 0, kCz + 12, 1.4f, 1.0f, 1.0f, 0, kWood));  // the crates
  v.push_back(at(kCx - 3.0, 1.0, kCz + 12.1, 0.9f, 0.8f, 0.8f, 0, kWood));
  v.push_back(at(kCx + 2.6, 0, kCz + 11.5, 1.0f, 1.0f, 1.0f, 0, kWood));
  v.push_back(at(kCx + 4.2, 0, kCz + 7.5, 0.25f, 3.7f, 0.25f, 2, kIron));         // the lamp's post
  v.push_back(at(kCx + 3.7, 3.62, kCz + 7.5, 1.1f, 0.08f, 0.08f, 0, kIron));      // and its arm
  v.push_back(at(kCx - 1.0, 0.45, kCz + 9.5, 0.45f, 0.45f, 0.45f, 1, kCopper));   // the urn
  v.push_back(at(kCx + 0.8, -0.03, kCz + 6.5, 3.5f, 0.05f, 2.2f, 0, kSlime));     // the pool
  v.push_back(at(kCx - 2.2, -0.03, kCz + 5.0, 4.0f, 0.04f, 3.0f, 0, kWetFloor));  // wet flags
  return v;
}

// The lamps' own glowing parts: drawn, but no lamp's shadow is cast by them.
std::vector<gr::Instance> glows() {
  std::vector<gr::Instance> v;
  v.push_back(at(kCx + 3.25, 3.5, kCz + 7.5, 0.16f, 0.16f, 0.16f, 1, kBulb));  // the lamp's bulb
  v.push_back(at(kCx - 6, 2.6, kCz + 9.3, 0.07f, 0.16f, 0.07f, 1, kFlame));    // the torches
  v.push_back(at(kCx + 6, 2.6, kCz + 9.3, 0.07f, 0.16f, 0.07f, 1, kFlame));
  v.push_back(at(kCx + 1.2, 2.6, kCz + 13.45, 1.2f, 0.06f, 0.06f, 0, kTube));  // the tube
  return v;
}

// THE LAMPS: two torches, the street lamp's spot, the failing tube, and (from `flash_s`) the
// muzzle flash at hip height left of the eye.
std::vector<gr::LocalLight> lamps(double flash_s) {
  std::vector<gr::LocalLight> v;
  for (int side = -1; side <= 1; side += 2) {
    gr::LocalLight t;
    t.position[0] = kCx + 6 * side;
    t.position[1] = 2.65;
    t.position[2] = kCz + 9.2;
    t.colour[0] = 1.0f;
    t.colour[1] = 0.55f;
    t.colour[2] = 0.2f;
    t.candela = 35;
    t.range_m = 9;
    t.radius_m = 0.1f;
    t.flicker = gr::Flicker::flame;
    t.rate_hz = 9;
    t.depth = 0.35f;
    t.seed = side < 0 ? 3 : 11;
    t.shadow = side < 0 ? 1 : 2;
    v.push_back(t);
  }
  gr::LocalLight s;
  s.position[0] = kCx + 3.25;
  s.position[1] = 3.4;
  s.position[2] = kCz + 7.5;
  s.direction[0] = -0.25f;
  s.direction[1] = -1;
  s.direction[2] = 0.1f;
  s.colour[0] = 1.0f;
  s.colour[1] = 0.85f;
  s.colour[2] = 0.65f;
  s.candela = 900;
  s.range_m = 16;
  s.radius_m = 0.08f;
  s.inner_deg = 30;
  s.outer_deg = 55;
  s.shadow = 3;
  v.push_back(s);
  gr::LocalLight tube;
  tube.position[0] = kCx + 1.2;
  tube.position[1] = 2.5;
  tube.position[2] = kCz + 13.3;
  tube.colour[0] = 0.8f;
  tube.colour[1] = 0.95f;
  tube.colour[2] = 1.1f;
  tube.candela = 60;
  tube.range_m = 6;
  tube.radius_m = 0.3f;
  tube.flicker = gr::Flicker::buzz;
  tube.rate_hz = 14;
  tube.depth = 0.5f;
  tube.seed = 5;
  tube.shadow = 4;
  v.push_back(tube);
  if (flash_s >= 0) {
    gr::LocalLight f;
    f.position[0] = kCx - 1.2;
    f.position[1] = 1.3;
    f.position[2] = kCz + 1.6;
    f.colour[0] = 1.0f;
    f.colour[1] = 0.8f;
    f.colour[2] = 0.45f;
    f.candela = 2.5e4f;
    f.range_m = 14;
    f.radius_m = 0.1f;
    f.flicker = gr::Flicker::pulse;
    f.born_s = flash_s;
    f.attack_s = 0.0015f;
    f.decay_s = 0.012f;
    f.shadow = 5;
    v.push_back(f);
  }
  return v;
}

// A scene in the lab's surfaces: `glows` cast no shadow; `shadows` gives the lamps theirs.
bool make_scene(ge::gpu::Device& d, Scene& s, const std::vector<gr::Instance>& props, uint32_t w,
                uint32_t h, const std::string& label, const std::vector<gr::Instance>& glow = {},
                bool shadows = false) {
  const std::vector<gr::Surface> surf = surfaces();
  return ge::lab::lit_scene::make_scene(d, s, surf, props, w, h, label, {}, 0, glow, shadows);
}

// The atlas as a picture: each tile's depth (near / distance) on a log scale, near white and far
// dark; black where nothing is cast.
gl::Picture atlas_picture(ge::gpu::Device& d, const gr::LampShadows& ls, int side) {
  const uint32_t n = ls.settings.size;
  const std::vector<float> depth = gr::read_red(d, ls.atlas, 0, n, n, gr::kDepthFormat);
  gl::Picture p = gl::create_picture(side, side, {0, 0, 0});
  const double near = ls.settings.near_m;
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      const size_t sx = static_cast<size_t>(x) * n / static_cast<size_t>(side);
      const size_t sy = static_cast<size_t>(y) * n / static_cast<size_t>(side);
      const float z = depth[sy * n + sx];
      const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(side) + x) * 4;
      uint8_t v = 0;
      if (z > 0) {
        // The distance from the lamp, 0.3 m white to 12 m dark.
        const double dist = near / z;
        const double k = std::clamp(1 - libm::log(dist / 0.3) / libm::log(40.0), 0.0, 1.0);
        v = static_cast<uint8_t>(40 + 215 * k);
      }
      // A tile's border, faint.
      const uint32_t tile = n / gr::kLampTilesAcross;
      const bool edge = sx % tile == 0 || sy % tile == 0;
      p.rgba[i] = edge ? 70 : v;
      p.rgba[i + 1] = edge ? 50 : v;
      p.rgba[i + 2] = edge ? 30 : v;
      p.rgba[i + 3] = 255;
    }
  }
  return p;
}

// What an eye in the courtyard adapts to: the geometric mean of the lamps' light on a white face
// at a few points of what it looks at (the flags under the torches and the lamp, the wall), as
// the eye weighs a scene of pools and shadows by its log.
double adapted(const std::vector<gr::LocalLight>& list, double t) {
  const double points[7][3] = {{kCx - 5, 0.2, kCz + 8}, {kCx + 5, 0.2, kCz + 8},
                               {kCx + 3, 0.2, kCz + 7}, {kCx, 2, kCz + 13},
                               {kCx, 0.1, kCz + 3},     {kCx - 9, 2, kCz + 13.4},
                               {kCx + 9, 2, kCz + 13.4}};
  double e = 0;
  for (const auto& p : points) e += libm::log(std::max(gr::lamp_lux_at(list, p, t), 1e-3));
  return libm::exp(e / 7);
}

double luma(const gl::Picture& p, int x, int y) {
  const size_t i = (static_cast<size_t>(y) * p.width + x) * 4;
  return 0.2126 * p.rgba[i] + 0.7152 * p.rgba[i + 1] + 0.0722 * p.rgba[i + 2];
}

// The clusters as a heat map over a dimmed picture: each pixel's cluster's lamp count, from 0
// (clear) through blue, green and yellow to red at 5 or more. Checks every pixel a lamp's sphere
// holds (off the depth read back) finds that lamp in its list; returns the misses.
int heat_map(const gr::LightBins& bins, const gr::FrameView& v, uint32_t w, uint32_t h,
             const std::vector<float>& depth, gl::Picture& pic, int& most) {
  const gr::CameraMatrices cam = gr::camera_matrices(v, static_cast<double>(w) / h);
  const gr::Mat4 inv = cam.inv_view_proj;
  int misses = 0;
  most = 0;
  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      const float z = depth[static_cast<size_t>(y) * w + x];
      const size_t i = (static_cast<size_t>(y) * w + x) * 4;
      if (z <= 0) {
        for (int k = 0; k < 3; ++k) pic.rgba[i + k] = static_cast<uint8_t>(pic.rgba[i + k] / 3);
        continue;
      }
      const float fx = static_cast<float>(x), fy = static_cast<float>(y);
      const float fw = static_cast<float>(w), fh = static_cast<float>(h);
      const float ndc[4] = {(fx + 0.5f) / fw * 2 - 1, 1 - (fy + 0.5f) / fh * 2, z, 1};
      float p4[4];
      gr::transform(inv, ndc, p4);
      const double rel[3] = {p4[0] / p4[3], p4[1] / p4[3], p4[2] / p4[3]};
      const int c = gr::cluster_of(cam, v.near_m, w, h, rel);
      uint32_t n = 0, first = 0;
      if (c >= 0) {
        first = bins.cells[static_cast<size_t>(c) * 2];
        n = bins.cells[static_cast<size_t>(c) * 2 + 1];
      }
      most = std::max(most, static_cast<int>(n));
      for (uint32_t l = 0; l < bins.head.info[0]; ++l) {
        const gr::GpuLight& g = bins.lights[l];
        const double dx = rel[0] - g.at[0], dy = rel[1] - g.at[1], dz = rel[2] - g.at[2];
        if ((dx * dx + dy * dy + dz * dz) * g.at[3] >= 0.98) continue;
        bool found = false;
        for (uint32_t q = 0; q < n; ++q) found = found || bins.refs[first + q] == l;
        if (!found) ++misses;
      }
      static const uint8_t ramp[6][3] = {{0, 0, 0},      {40, 90, 230},  {30, 190, 120},
                                         {230, 220, 40}, {245, 140, 30}, {230, 40, 40}};
      const uint8_t* col = ramp[std::min<uint32_t>(n, 5)];
      for (int k = 0; k < 3; ++k) {
        const double base = pic.rgba[i + k] * 0.35;
        pic.rgba[i + k] = static_cast<uint8_t>(n == 0 ? base : base * 0.4 + col[k] * 0.6);
      }
    }
  }
  return misses;
}

void flicker_chart(gl::Lab& lab) {
  gl::LineChart c;
  c.frame = {"Each flicker's level over two seconds",
             "render/lights.hpp's flicker_level; 1 the light's full candela",
             "t (s)",
             "level",
             960,
             420};
  c.spikes = false;
  const std::vector<gr::LocalLight> all = lamps(0.5);
  const char* names[] = {"torch (flame)", "torch, another seed", "street lamp (steady)",
                         "failing tube (buzz)", "muzzle flash (pulse at 0.5 s)"};
  gr::LocalLight steps;
  steps.flicker = gr::Flicker::steps;
  steps.steps = "mmnmmommommnonmmonqnmmo";
  steps.rate_hz = 10;
  for (size_t k = 0; k <= all.size(); ++k) {
    const gr::LocalLight& l = k < all.size() ? all[k] : steps;
    gl::Trace t;
    t.label = k < all.size() ? names[k] : "steps \"mmnmmommommnonmmonqnmmo\" at 10 Hz";
    for (int i = 0; i <= 2000; ++i) {
      t.x.push_back(i * 0.001);
      t.y.push_back(gr::flicker_level(l, i * 0.001));
    }
    c.traces.push_back(t);
  }
  const gl::Chart chart = gl::line_chart(c);
  lab.shot({"flicker",
            "EVERY FLICKER OVER TWO SECONDS",
            "The level of each kind of light in the courtyard, sampled every millisecond: two "
            "torches' flames (two octaves of smooth noise and a slow breath, each its own seed), "
            "the steady street lamp, the failing tube's dark gaps, the muzzle flash's rise in "
            "1.5 ms and fall by e every 12 ms, and a step pattern of letters.",
            "the two torches never in step and never under 0.65; the tube mostly whole with short "
            "drops near 0; one sharp spike at 0.5 s gone within 0.1 s; the steps' plateaus",
            {}},
           gl::to_picture(chart.plot), Det::measured);
}

void checked(gl::Lab& lab, int errors, int late) {
  lab.metric("gpu-errors", errors, "count");
  lab.metric("pipelines-after-load", late, "count");
  if (errors != 0 || late != 0) {
    lab.fail(std::to_string(errors) + " GPU errors, " + std::to_string(late) +
                 " pipelines after the load",
             "Read the gpu: lines above; add any pipeline to the scene's manifest "
             "(engine/lab/labs/gpu_lights_lab.cpp). Skill: render.");
  }
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 9 / 16;
  flicker_chart(lab);

  // The binning's cost on the CPU, for a crowd of lamps (a town at night): measured.
  {
    std::vector<gr::LocalLight> town;
    for (int i = 0; i < 400; ++i) {
      gr::LocalLight l;
      l.position[0] = kCx + (i % 20 - 10) * 9.0;
      l.position[1] = 4;
      l.position[2] = kCz + 6 + std::floor(i / 20.0) * 9.0;
      l.range_m = 12;
      town.push_back(l);
    }
    const gr::FrameView v = view_at(kCx, 1.7, kCz, 0, -0.05, true, 1);
    gr::LightBins bins;
    gr::reserve_bins(bins);
    gr::bin_lights(bins, town, v, w, h);
    const auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < 20; ++k) gr::bin_lights(bins, town, v, w, h);
    const double us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() /
        20;
    lab.metric("town.lights", bins.head.info[0], "count");
    lab.metric("town.refs", bins.ref_count, "count", Det::measured);
    lab.metric("town.dropped", bins.dropped, "count");
    lab.metric("town.bin-us", us, "us", Det::measured);
  }

  auto device = ge::gpu::create_device({.label = "gpu-lights", .validation = 1});
  if (!device) {
    if (gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    placeholders(lab,
                 {"night-unlit", "night-unshadowed", "night-lit", "atlas", "clusters", "flash",
                  "spheres-noon", "spheres-lamp"},
                 w, h);
    return;
  }
  ge::gpu::Device& d = *device;
  Scene s;
  if (!make_scene(d, s, courtyard(), w, h, "gpu-lights", glows(), true)) {
    lab.fail("the scene's pipelines did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }

  const double t = 2.0;
  const std::vector<gr::LocalLight> lit_list = lamps(-1);
  gr::FrameView yard = view_at(kCx, 1.7, kCz, 0, -0.06, true, t);
  yard.lamp_lux = adapted(lit_list, t);
  lab.metric("yard.lamp-lux", yard.lamp_lux, "lx", Det::measured);
  const gl::Picture unlit = draw(d, s, yard, {});
  const gl::Picture lit = draw(d, s, yard, lit_list);
  const uint32_t dealt = s.lamp_shadows.used, first_drawn = s.lamp_shadows.drawn;
  lab.metric("yard.shadow-lamps", s.lamp_shadows.lamps, "count", Det::measured);
  lab.metric("yard.shadow-tiles", dealt, "count", Det::measured);
  // The same frame again: every lamp stood still and nothing moved, so no tile is redrawn.
  (void)draw(d, s, yard, lit_list);
  const uint32_t still_drawn = s.lamp_shadows.drawn;
  lab.metric("yard.tiles-redrawn-still", still_drawn, "count", Det::measured);
  if (first_drawn != dealt || still_drawn != 0) {
    lab.fail("the still courtyard redrew " + std::to_string(still_drawn) + " of " +
                 std::to_string(dealt) + " shadow tiles (" + std::to_string(first_drawn) +
                 " the first frame)",
             "Check deal_lamp_tiles' due test (render/lamp_shadows.cpp): a tile is due only when "
             "new, its lamp moved, or a mover touches its view. Skill: render.");
  }
  const gl::Picture atlas = atlas_picture(d, s.lamp_shadows, static_cast<int>(h));
  // The lamps without their shadows, on a scene of their own.
  gl::Picture plain;
  int plain_errors = 0, plain_late = 0;
  {
    auto dev = ge::gpu::create_device({.label = "gpu-lights.plain", .validation = 1});
    Scene p;
    if (!dev || !make_scene(*dev, p, courtyard(), w, h, "gpu-lights.plain", glows(), false)) {
      lab.fail("the unshadowed scene did not build", "Read the gpu: lines above. Skill: render.");
      return;
    }
    plain = draw(*dev, p, yard, lit_list);
    plain_errors = static_cast<int>(dev->errors.load());
    plain_late = static_cast<int>(dev->pipelines_after_load);
  }
  lab.metric("yard.lights", s.lights.bins.head.info[0], "count", Det::measured);
  lab.metric("yard.refs", s.lights.bins.ref_count, "count", Det::measured);
  const std::vector<std::pair<std::string, std::string>> inputs = {
      {"night", "moonless, sun 20 deg down"}, {"t", "2 s"}};
  lab.shot({"night-unlit", "THE COURTYARD, UNLIT",
            "A stone wall and pillars, crates, an iron post, a copper urn, a slime pool and wet "
            "flags at the bottom of a moonless night, seen from 1.7 m: the dark sky's own light "
            "alone (render/night.hpp), with the lamps' glowing parts drawn but lighting nothing, "
            "at the exposure of an eye adapted to the lit courtyard (the next picture).",
            "the scene black but for the sky; the bulb, the flames, the tube and the slime's "
            "faint glow with nothing round them lit",
            inputs},
           unlit, Det::measured);
  lab.shot({"night-unshadowed", "THE COURTYARD BY ITS LAMPS, NO SHADOWS",
            "The same night lit by two flame-flickered torches on the pillars (35 cd, 9 m), the "
            "street lamp's spot under its arm (900 cd, a 30 to 55 degree cone, 16 m), and a "
            "failing tube on the wall (60 cd, 6 m): each pixel lit by the lamps listed in its "
            "cluster (render/lights.hpp), through the physically based surfaces "
            "(wgsl/surface.wgsl), with no lamp casting a shadow.",
            "warm pools on the pillars and the flags under each torch, falling off by the inverse "
            "square; the spot's hard-edged disc under the arm; highlights on the iron post, the "
            "copper urn and the wet flags, none on the matte stone; light reaching the wall "
            "straight through the pillars and the crates",
            inputs},
           plain, Det::measured);
  lab.shot({"night-lit", "THE COURTYARD BY ITS LAMPS, WITH THEIR SHADOWS",
            "The same lamps casting (render/lamp_shadows.hpp): each torch and the tube a point "
            "light's six cube faces, the spot one tile round its axis, dealt from one depth atlas "
            "and read through four rotated comparison taps a pixel. The lamps' own glowing parts "
            "cast nothing.",
            "the pillars throwing wide dark wedges on the wall behind them from the torches just "
            "in front; the crates' and the urn's shadows on the flags and the wall, softer the "
            "further they fall; the post's thin shadow and the arm's on the flags under the spot; "
            "no light leaking under a crate, no acne on a lit face, no seam where two cube faces "
            "meet",
            inputs},
           lit, Det::measured);
  lab.shot({"atlas", "THE LAMPS' SHADOW ATLAS",
            "The depth atlas the frame above read, 8 x 8 tiles: the two torches' and the tube's "
            "cube faces (+x, -x, +y, -y, +z, -z, left to right in a run) and the spot's one tile, "
            "each pixel the distance from its lamp to the nearest caster on a "
            "log scale (0.3 m white, 12 m dark), black where nothing casts and in the tiles no "
            "lamp holds.",
            "three runs of six tiles and one more in the top rows; the pillar filling the torch's "
            "+z face nearly "
            "white; the floor's face (-y) black (the flags are terrain, which the lamps do not "
            "draw); the spot's tile the crates and the urn seen from above",
            inputs},
           atlas, Det::measured);
  // The shadows darken a share of the courtyard, and never brighten it.
  {
    int darker = 0, brighter = 0;
    for (int y = 0; y < lit.height; ++y) {
      for (int x = 0; x < lit.width; ++x) {
        const double a = luma(plain, x, y), b = luma(lit, x, y);
        if (b + 4 < a) ++darker;
        if (b > a + 4) ++brighter;
      }
    }
    const double share =
        static_cast<double>(darker) / (static_cast<double>(lit.width) * lit.height);
    lab.metric("yard.shadowed-share", share, "share", Det::measured);
    lab.metric("yard.shadow-brighter-pixels", brighter, "count", Det::measured);
    if (share < 0.01) {
      lab.fail("the lamps' shadows darken " + fixed(100 * share, 2) + " % of the courtyard",
               "Check prepare_lamp_shadows ran, frame_group bound the atlas (binding 13) and "
               "lamp_shadow in wgsl/lights.wgsl reads the tile's slot. Skill: render.");
    }
  }
  // A crate shoved half a metre: the tiles that see it redrawn, the rest kept.
  {
    std::vector<gr::Instance> moved = courtyard();
    gr::Instance& crate = moved[kSmallCrate];
    crate.position[0] += 0.5;
    gr::set_instances(d, s.props, 0, moved);
    s.movers.push_back({crate.position[0], 0.5, crate.position[2], 0.9});
    (void)draw(d, s, yard, lit_list);
    const uint32_t moved_drawn = s.lamp_shadows.drawn;
    // And back: the crate moves again, so the tiles that see it are redrawn once more.
    s.movers.back()[0] -= 0.5;
    gr::set_instances(d, s.props, 0, courtyard());
    (void)draw(d, s, yard, lit_list);
    s.movers.clear();
    lab.metric("yard.tiles-redrawn-mover", moved_drawn, "count", Det::measured);
    if (moved_drawn == 0 || moved_drawn >= dealt) {
      lab.fail("a moved crate redrew " + std::to_string(moved_drawn) + " of " +
                   std::to_string(dealt) + " shadow tiles",
               "Check deal_lamp_tiles' mover test against the tiles' planes "
               "(render/lamp_shadows.cpp). Skill: render.");
    }
  }

  // Lit no darker anywhere, and brighter where the lamps reach.
  int darker = 0;
  double gain = 0;
  for (int y = 0; y < lit.height; ++y) {
    for (int x = 0; x < lit.width; ++x) {
      const double a = luma(unlit, x, y), b = luma(lit, x, y);
      if (b + 2 < a) ++darker;
      gain += b - a;
    }
  }
  gain /= static_cast<double>(lit.width) * lit.height;
  lab.metric("yard.darker-pixels", darker, "count", Det::measured);
  lab.metric("yard.mean-gain", gain, "levels", Det::measured);
  if (gain < 4) {
    lab.fail("the lamps brighten the courtyard by " + fixed(gain, 2) + " levels a pixel",
             "Check prepare_lights ran before the draw and frame_group bound Lights::buffer "
             "(render/lights.hpp). Skill: render.");
  }

  // The clusters off the depth.
  {
    const std::vector<float> depth = gr::read_depth(d, s.targets);
    gl::Picture heat = lit;
    int most = 0;
    const int misses = heat_map(s.lights.bins, yard, w, h, depth, heat, most);
    lab.metric("yard.cluster-misses", misses, "count", Det::measured);
    lab.metric("yard.cluster-most", most, "count", Det::measured);
    if (misses > 0) {
      lab.fail(std::to_string(misses) + " pixels inside a lamp's sphere whose cluster lacks it",
               "Check bin_lights' tile range and slice_of against cluster_index in "
               "wgsl/lights.wgsl. Skill: render.");
    }
    lab.shot({"clusters", "THE CLUSTERS' LAMPS",
              "Each pixel coloured by how many lamps its cluster lists (16 x 9 tiles, 24 slices "
              "by depth): none clear over the dimmed picture, 1 blue, 2 green, 3 yellow, 4 "
              "orange, 5 or more red. Checked off the depth read back: every pixel inside a "
              "lamp's sphere finds that lamp in its list.",
              "colour only round the lamps' spheres, stepping in tiles and slices; the far wall "
              "and the sky clear past every lamp's range",
              inputs},
             heat, Det::measured);
  }

  // The flash: four moments of a muzzle flash fired at t.
  {
    const int fw = static_cast<int>(w) / 2, fh = static_cast<int>(h) / 2;
    gl::Picture sheet = gl::create_picture(fw * 2, fh * 2, {0, 0, 0});
    const double ages[4] = {-0.001, 0.0015, 0.012, 0.1};
    for (int k = 0; k < 4; ++k) {
      gr::FrameView v = yard;
      v.time_s = t + ages[k];  // the eye keeps its adaptation through the flash
      const gl::Picture p = draw(d, s, v, lamps(t));
      gl::Picture half = gl::create_picture(fw, fh, {0, 0, 0});
      const auto hw = static_cast<size_t>(fw), pw = static_cast<size_t>(p.width);
      for (size_t y = 0; y < static_cast<size_t>(fh); ++y) {
        for (size_t x = 0; x < hw; ++x) {
          for (size_t c = 0; c < 4; ++c) {
            int sum = 0;
            for (size_t q = 0; q < 4; ++q) {
              sum += p.rgba[((y * 2 + q / 2) * pw + x * 2 + q % 2) * 4 + c];
            }
            half.rgba[(y * hw + x) * 4 + c] = static_cast<uint8_t>(sum / 4);
          }
        }
      }
      paste(sheet, half, (k % 2) * fw, (k / 2) * fh);
    }
    lab.shot({"flash", "A MUZZLE FLASH'S FIRST TENTH OF A SECOND",
              "The courtyard as a shot is fired at hip height left of the eye (25 000 cd at its "
              "peak, 14 m): 1 ms before, at its peak 1.5 ms in, one decay (12 ms) later, and at "
              "100 ms; read left to right, top to bottom. A pulse is a lamp like any other, its "
              "level a pure function of the frame's time.",
              "the first frame as night-lit; the second washed warm-white across the flags, the "
              "crates and the pillars' near faces, the urn and the post glinting; the third a "
              "third of that; the fourth back to the torches",
              inputs},
             sheet, Det::measured);
  }

  // THE SPHERES: a sheet of them in front of the wall, by day and by one lamp.
  {
    Scene sp;
    auto dev2 = ge::gpu::create_device({.label = "gpu-lights.spheres", .validation = 1});
    std::vector<gr::Instance> balls;
    for (int r = 0; r < kSheetRows; ++r) {
      for (int c = 0; c < kSheetCols; ++c) {
        balls.push_back(at(kCx + (c - 2.5) * 1.1, 0.55 + (kSheetRows - 1 - r) * 1.1, kCz + 6, 0.5f,
                           0.5f, 0.5f, 1, kSheet + r * kSheetCols + c));
      }
    }
    balls.push_back(at(kCx, 0, kCz + 8, 12, 4, 0.5f, 0, kStone));
    if (!dev2 || !make_scene(*dev2, sp, balls, w, h, "gpu-lights.spheres")) {
      lab.fail("the spheres' scene did not build", "Read the gpu: lines above. Skill: render.");
      return;
    }
    const gr::FrameView noon = view_at(kCx, 1.7, kCz, 0, 0.02, false, 1);
    lab.shot({"spheres-noon",
              "THE SURFACES UNDER THE NOON SUN",
              "Spheres of each surface, roughness rising left to right from 0.05 to 1: a red "
              "dielectric (4 % reflectance) on top, gold metal in the middle, and the presets "
              "below (matte grey, stone, wood, iron, slime, a wet floor), under the sun 63 "
              "degrees up and the sky.",
              "the dielectric's sharp white highlight spreading and fading toward the right, its "
              "red body unchanged; the metal's highlight gold, its body dark where it mirrors the "
              "dark ground and blue-tinted where it mirrors the sky; the matte grey with no "
              "highlight at all",
              {{"sun", "63 deg"}}},
             draw(*dev2, sp, noon, {}), Det::measured);
    gr::LocalLight one;
    one.position[0] = kCx - 2.5;
    one.position[1] = 3.4;
    one.position[2] = kCz + 3.2;
    one.colour[0] = 1.0f;
    one.colour[1] = 0.9f;
    one.colour[2] = 0.75f;
    one.candela = 120;
    one.range_m = 14;
    gr::FrameView night = view_at(kCx, 1.7, kCz, 0, 0.02, true, 1);
    const double sheet_mid[3] = {kCx, 1.6, kCz + 5.5};
    night.lamp_lux = gr::lamp_lux_at(std::span(&one, 1), sheet_mid, 1);
    lab.shot({"spheres-lamp",
              "THE SURFACES UNDER ONE LAMP AT NIGHT",
              "The same spheres at the bottom of a moonless night, lit by one warm lamp (120 cd) "
              "up and to the left: the lamps' light through the same GGX lobe and Lambert body as "
              "the sun's.",
              "each sphere lit from the upper left and falling into darkness on its right; the "
              "smooth ones holding a small bright point of the lamp, the rough ones a broad sheen; "
              "the far spheres dimmer by the inverse square",
              {{"lamp", "120 cd"}}},
             draw(*dev2, sp, night, std::vector<gr::LocalLight>{one}), Det::measured);
    checked(lab, static_cast<int>(d.errors.load() + dev2->errors.load()) + plain_errors,
            static_cast<int>(d.pipelines_after_load + dev2->pipelines_after_load) + plain_late);
  }
}

const gl::LabSpec kSpec{
    "gpu-lights",
    gl::LabKind::picture,
    "engine",
    "engine/render/lights.hpp",
    "render",
    "Do the local lights light a night scene by their units, flicker as their kind does, land "
    "in every cluster they reach, and do the surfaces answer the sun and a lamp each in its way?",
    {{"width", "640", "a view's width, px (16:9)"}},
    "--width=256",
    run};
GE_LAB(kSpec);

}  // namespace
