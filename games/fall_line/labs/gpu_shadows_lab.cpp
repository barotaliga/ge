// LAB `shadows`: the ground's own shadow, the maps themselves (engine/render/horizon.hpp). It
// generates a seed's map, makes its terrain on a headless device, and at each --hours hour bakes
// the horizon maps the sun needs, reads both back and draws the map from straight above: the
// ground shaded by the sun, darkened and blued where the horizon maps put it in shadow, exactly
// as the terrain's shader blends them. The share of the map in shadow at each hour is beside it.
//
// The bake DECISION is deterministic and needs no GPU: the sun swept from --from to --to hours
// in --step steps, counting the maps baked (two to start, then one each time the sun's bearing
// crosses a step, none while it stays inside one). Pictures and shares are measured. Without an
// adapter it notes that and draws placeholders, so the digest is the same on every row;
// GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the horizon maps over a day, from above, and the bake counter of a sun sweep
// ge:keywords lab shadows horizon-map ridge-shadow sun-sweep bakes shadow-share picture
// ge:keywords fall-line terrain-shadow
// ge:skill render labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "fall_line/app/look.hpp"
#include "fall_line/app/views.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "render/frame.hpp"
#include "render/horizon.hpp"
#include "render/sky.hpp"
#include "render/terrain.hpp"

namespace {

using ge::lab::Det;
namespace r = ge::render;

bool env_on(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool on = _dupenv_s(&v, &n, name) == 0 && v && std::strcmp(v, "1") == 0;
  std::free(v);
  return on;
#else
  const char* v = std::getenv(name);
  return v && std::strcmp(v, "1") == 0;
#endif
}

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// terrain.wgsl's `horizon_light` on the CPU, over the maps read back.
double light_at(const r::Horizon& h, const std::vector<float> maps[2], int x, int y,
                double sun_tan) {
  const size_t i = static_cast<size_t>(y) * static_cast<size_t>(h.width) + static_cast<size_t>(x);
  const bool odd = h.blend[1] > 0.5f;
  const double lo = maps[odd ? 1 : 0][i], hi = maps[odd ? 0 : 1][i];
  const double w = h.blend[2];
  auto step = [w](double d) {
    const double t = std::clamp((d + w) / (2 * w), 0.0, 1.0);
    return t * t * (3 - 2 * t);
  };
  return step(sun_tan - lo) + (step(sun_tan - hi) - step(sun_tan - lo)) * h.blend[0];
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const std::vector<double> hours = lab.arg_nums("hours");
  const double from = lab.arg_num("from"), to = lab.arg_num("to"), step = lab.arg_num("step");
  if (hours.empty() || step <= 0 || to < from) {
    lab.fail("--hours needs at least one hour, --step must be positive and --to at least --from",
             "Pass --hours=9,12,15 --from=7 --to=17 --step=0.05.");
    return;
  }
  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const ge::mountain::mapgen::Level level = ge::mountain::mapgen::generate_level(seed, opts);

  // The sweep: the bake decision alone, on the CPU.
  r::Horizon sweep;
  int frames = 0;
  const int count = static_cast<int>(std::floor((to - from) / step + 1e-9)) + 1;
  for (int i = 0; i < count; ++i) {
    r::FrameView v;
    fall_line::app::day_view(level, from + i * step, v);
    r::prepare_horizon(nullptr, sweep, v);
    ++frames;
  }
  lab.metric("sweep-frames", frames, "count");
  lab.metric("sweep-bakes", sweep.bakes, "count");
  lab.metric("bearing-step", r::kHorizonBearingStep * 180 / 3.14159265358979323846, "deg");

  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  auto shot_of = [&](double hr) {
    const std::string id = "hour-" + fixed(hr, 1);
    return ge::lab::Shot{
        id,
        "SEED " + seed_s + " AT " + fixed(hr, 1) + " H, FROM ABOVE",
        "The map from straight above, north up: the ground shaded by the sun's angle, dark blue "
        "where the horizon maps put it in the shadow of higher ground toward the sun.",
        "shadows fall away from the sun and lengthen as it sinks; ridges cast them over the "
        "valleys beside; no speckle on open slopes; the edge glides rather than steps between "
        "hours",
        {{"seed", seed_s}, {"hour", fixed(hr, 2)}}};
  };

  auto device = ge::gpu::create_device({.label = "shadows", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (double hr : hours) {
      ge::lab::Picture none = ge::lab::create_picture(320, 240, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(hr), none, Det::measured);
    }
    // The digest hashes in call order: the shots, then this, as the device path does.
    lab.metric("gpu-errors", 0, "count");
    return;
  }

  ge::gpu::PipelineSet pipelines;
  const wgpu::BindGroupLayout frame = r::frame_layout(*device);
  const std::vector<uint8_t> albedo = fall_line::app::bake_albedo(level);
  r::Terrain terrain;
  r::TerrainSource source;
  source.height = &level.ground;
  source.albedo = &albedo;
  source.levels = 2;
  r::create_terrain(*device, pipelines, terrain, source, frame);
  if (!pipelines.finish(*device)) {
    lab.fail("the terrain's pipelines did not build", "Read the gpu: lines above. Skill: gpu.");
    return;
  }
  ge::gpu::end_load(*device);
  r::FrameRecorder rec;
  rec.init(*device, "shadows.timer", 1);
  const r::Horizon& h = terrain.horizon;
  lab.note("horizon maps " + std::to_string(h.width) + "x" + std::to_string(h.height) + " at " +
           fixed(h.cell, 1) + " m a texel, marched to " + fixed(h.march.range_m, 0) + " m");

  for (double hr : hours) {
    r::FrameView v;
    fall_line::app::day_view(level, hr, v);
    rec.begin_frame(*device);
    r::prepare_terrain(*device, terrain, v);
    wgpu::CommandEncoder encoder = device->device.CreateCommandEncoder();
    r::encode_terrain_horizon(terrain, pipelines, rec, encoder);
    rec.end_frame(encoder);
    wgpu::CommandBuffer commands = encoder.Finish();
    device->queue.Submit(1, &commands);
    rec.after_submit(*device);
    rec.wait(*device);
    lab.metric("hour-" + fixed(hr, 1) + ".bake-ms", rec.stats.passes[0].gpu_ms, "ms",
               Det::measured);
    const std::vector<float> maps[2] = {r::read_horizon(*device, h, 0),
                                        r::read_horizon(*device, h, 1)};
    double sun[3];
    r::sun_direction(v, sun);
    // As the terrain's shader takes it: the sun's height over its run in the plan.
    const double sun_tan = sun[1] / std::max(std::sqrt(sun[0] * sun[0] + sun[2] * sun[2]), 1e-4);
    ge::lab::Picture pic = ge::lab::create_picture(h.width, h.height, {0, 0, 0});
    size_t dark = 0;
    for (int y = 0; y < h.height; ++y) {
      for (int x = 0; x < h.width; ++x) {
        const double wx = h.origin_x + x * h.cell, wz = h.origin_z + y * h.cell;
        const ge::core::FieldSlope g = ge::core::field_gradient(level.ground, wx, wz);
        const double n[3] = {-g.gx, 1, -g.gz};
        const double nl = std::max((n[0] * sun[0] + n[1] * sun[1] + n[2] * sun[2]) /
                                       std::sqrt(g.gx * g.gx + 1 + g.gz * g.gz),
                                   0.0);
        const double lit = light_at(h, maps, x, y, sun_tan);
        if (lit < 0.5) ++dark;
        const double s = 0.25 + 0.75 * nl * lit;
        const auto c = [&](double base, double shade) {
          return static_cast<uint8_t>(std::clamp(base * s + shade * (1 - lit), 0.0, 255.0));
        };
        // North (+z) up: row 0 of the picture is the map's far edge.
        ge::lab::set_pixel(pic, x, h.height - 1 - y, {c(250, 0), c(250, 10), c(255, 70)});
      }
    }
    lab.metric("hour-" + fixed(hr, 1) + ".shadow-share",
               100.0 * static_cast<double>(dark) / (static_cast<double>(h.width) * h.height), "%",
               Det::measured);
    lab.shot(shot_of(hr), pic, Det::measured);
  }
  lab.metric("gpu-errors", device->errors.load(), "count");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while baking",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "shadows",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/render/horizon (the ground's own shadow) over Fall Line's map",
    "render",
    "Where does the mountain shade itself through a day, and how often are its maps rebaked?",
    {
        {"seed", "38", "the map's seed"},
        {"hours", "9,12,15", "the solar hours to draw the maps at, comma-separated"},
        {"from", "7", "the sweep's first hour"},
        {"to", "17", "the sweep's last hour"},
        {"step", "0.05", "the sweep's step, hours"},
    },
    "--hours=12",
    run,
};
GE_LAB(kSpec);

}  // namespace
