// LAB `far-land`: THE FAR LAND (engine/render/far_land.hpp) LOOKED AT: each kind round a site
// 60 m across (a small world's circle), seen from an eye standing high over the site's floor, as
// a player on a world's roof would see it over its walls: by DAY (the sun 35 degrees up), LOW
// (the sun 5 degrees up: the ranges' shadows long across the valleys) and by NIGHT (the sun gone,
// a full moon 35 degrees up). The eye looks along the bearing where the ranges stand highest.
// For the first kind, the same view under `--warm`'s temperatures, so the snow line is seen to
// follow the air: colder brings the snow down the ranges and onto the lake, warmer lifts it off.
//
// The digest holds each kind's mesh (its vertices and triangles, its highest point, its snow line
// and its water's level, all from the CPU's build, the same bits on every machine); the pictures
// are the GPU's and measured.
//
// ge:summary Lab: each far-land kind by day, low sun and night, and the snow line by temperature
// ge:keywords lab far-land farland skybox mountains snowline warm temperature kinds picture
// ge:skill render labs
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "render/bloom.hpp"
#include "render/cloud_kinds.hpp"
#include "render/clouds.hpp"
#include "render/far_land.hpp"
#include "render/frame.hpp"
#include "render/night.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/sky_style.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace ls = ge::lab::liquid_scene;
using gl::Det;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;
constexpr double kEyeUp = 60;  // m over the site's floor

// The sun's elevation and its bearing off the eye's heading (degrees): from the side by day, low
// and ahead to one side for the long shadows, and by night the moon there.
struct Light {
  const char* id;
  const char* title;
  double sun_el, sun_off;
  bool night;
};
constexpr Light kLights[] = {{"day", "BY DAY", 35, 120, false},
                             {"low", "LOW SUN", 5, 55, false},
                             {"night", "BY NIGHT", -18, 220, true}};

struct Frame {
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms;
  gr::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group;
  gr::Sky sky;
  gr::CloudPass clouds;
  gr::Bloom bloom;
  gr::Post post;
  gr::FarLand land;
  gr::FrameRecorder rec;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

std::string fixed(double v, int places) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.*f", places, v);
  return buf;
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string item =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!item.empty()) out.push_back(item);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

// The bearing (rad, a heading) where the ranges stand highest between 1.5 and 6 times the kind's
// first range's distance: where a reader looks first.
double highest_bearing(const gr::FarMesh& mesh, double near_m) {
  int best = 0;
  double top = -1e30;
  const int b = mesh.bearings;
  for (int i = 0; i < b; i += std::max(1, b / 32)) {
    for (int j = 0; j < mesh.rings; ++j) {
      const gr::FarVertex& v = mesh.vertices[size_t(j) * b + i];
      const double r = std::sqrt(double(v.pos[0]) * v.pos[0] + double(v.pos[2]) * v.pos[2]);
      if (r < 1.5 * near_m || r > 6 * near_m) continue;
      if (v.pos[1] > top) {
        top = v.pos[1];
        best = i;
      }
    }
  }
  return 2 * kPi * best / b;
}

// The view: the eye high over the site looking along `heading`, a layer of fair heaps over the
// land, the kind's sky, the light's sun (and at night the moon).
gr::FrameView view_of(const gr::FarLandKind& kind, const Light& light, double heading,
                      double floor_y) {
  gr::FrameView v;
  v.eye[0] = 0;
  v.eye[1] = floor_y + kEyeUp;
  v.eye[2] = 0;
  v.heading = heading;
  v.pitch = 1.5 * kDeg;
  v.fov_y = 0.9;
  v.near_m = 0.1;
  v.altitude_m = 400;
  v.time_s = 600;
  gr::sky_style_named(kind.sky, v.style);
  v.sun_elevation = light.sun_el * kDeg;
  v.sun_azimuth = heading + light.sun_off * kDeg;
  if (const gr::CloudKind* ck = gr::cloud_kind_named("cumulus")) {
    gr::CloudLayer& l = v.clouds.layers[0];
    const double depth = 700;
    l.base_m = floor_y + 2200;
    l.top_m = l.base_m + depth;
    gr::take_form(*ck, depth, l);
    l.cover = 0.22;
    l.cell_m = 0.5 * (ck->cell[0] + ck->cell[1]);
    l.patch = std::max(l.patch, 0.5);
    l.warp = 1.0;
    v.clouds.count = 1;
  }
  v.clouds.quarter = false;
  v.clouds.spread = false;
  if (light.night) {
    const gr::Moon moon{35 * kDeg, heading + (light.sun_off - 180) * kDeg, 0.98, gr::kMoonMeanKm};
    gr::set_night(v, moon, 46, 80);
  }
  return v;
}

gl::Picture draw(ge::gpu::Device& d, Frame& f, const gr::FrameView& v, double warm) {
  f.rec.begin_frame(d);
  gr::fill_uniforms(f.staged, v, f.w, f.h);
  d.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
  gr::prepare_sky(f.sky, v);
  gr::prepare_far_land(d, f.land, v, warm);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_sky_tables(f.sky, f.pipelines, f.rec, e);
  gr::encode_far_land(f.land, f.pipelines, f.rec, e, f.targets, f.frame, true);
  gr::encode_sky(f.sky, f.pipelines, f.rec, e, f.targets, f.frame);
  gr::encode_clouds(f.clouds, f.pipelines, f.rec, e, f.targets, f.frame, v, f.staged);
  gr::encode_bloom(f.bloom, f.pipelines, f.rec, e, 0);
  gr::encode_post(f.post, f.pipelines, f.rec, e, f.frame, f.post_group, f.out.view);
  f.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  f.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, f.out);
  return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
}

void run(gl::Lab& lab) {
  std::vector<const gr::FarLandKind*> kinds;
  for (const std::string& name : split(lab.arg("kinds"))) {
    if (name == "all") {
      for (const gr::FarLandKind& k : gr::far_land_kinds()) kinds.push_back(&k);
    } else if (const gr::FarLandKind* k = gr::find_far_land(name)) {
      kinds.push_back(k);
    } else {
      std::string list;
      for (const gr::FarLandKind& each : gr::far_land_kinds()) list += std::string(" ") + each.name;
      lab.fail("--kinds names no far land: " + name, "next: pick from" + list + ", or all.");
      return;
    }
  }
  const std::vector<double> warms = lab.arg_nums("warm");
  const double detail = std::clamp(lab.arg_num("detail"), 0.1, 2.0);
  const int bearings = std::max(16, static_cast<int>(640 * detail));
  const int rings = std::max(16, static_cast<int>(384 * detail));
  const uint32_t w = static_cast<uint32_t>(std::max(64.0, lab.arg_num("width")));
  const uint32_t h = w * 9 / 16;

  // THE MESHES, on the CPU: the digest's half.
  gr::FarSite site;
  site.radius_m = 60;
  site.floor_y = 0;
  site.seed = 7;
  std::vector<gr::FarMesh> meshes;
  gl::LabTable table;
  table.title = "THE KINDS' LAND";
  table.columns = {"kind", "what", "highest m", "snow line m", "water m", "vertices"};
  for (const gr::FarLandKind* k : kinds) {
    const auto t0 = std::chrono::steady_clock::now();
    meshes.push_back(gr::build_far_land(*k, site, bearings, rings));
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const gr::FarMesh& m = meshes.back();
    const double snow = gr::far_snow_line(*k, m, 0);
    const std::string key = std::string(k->name);
    lab.metric(key + ".vertices", double(m.vertices.size()), "count");
    lab.metric(key + ".triangles", double(m.indices.size()) / 3, "count");
    lab.metric(key + ".highest", std::round(m.high_y * 10) / 10, "m");
    lab.metric(key + ".snow-line", std::round(snow * 10) / 10, "m");
    lab.metric(key + ".water", k->water >= 0 ? std::round(m.water_y * 10) / 10 : 0, "m");
    lab.metric(key + ".build", ms, "ms", Det::measured);
    table.rows.push_back({k->name, k->what, fixed(m.high_y, 0), fixed(snow, 0),
                          k->water >= 0 ? fixed(m.water_y, 0) : "none",
                          std::to_string(m.vertices.size())});
  }
  lab.table(table);

  std::vector<std::string> ids;
  for (const gr::FarLandKind* k : kinds) {
    for (const Light& l : kLights) ids.push_back(std::string(k->name) + "-" + l.id);
  }
  if (!kinds.empty()) {
    for (double warm : warms) ids.push_back(std::string(kinds[0]->name) + "-warm" + fixed(warm, 0));
  }

  auto device = ge::gpu::create_device({.label = "far-land", .validation = 1});
  if (!device) {
    if (ls::gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: no pictures.");
    for (const std::string& id : ids) ls::placeholders(lab, {id.c_str()});
    ls::checked(lab, 0, 0);
    return;
  }
  ge::gpu::Device& d = *device;
  Frame f;
  f.w = w;
  f.h = h;
  f.targets = gr::create_targets(d, "far-land", w, h);
  f.uniforms = gr::make_buffer(d, "far-land.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  f.layout = gr::frame_layout(d);
  gr::create_sky(d, f.pipelines, f.sky, f.uniforms, f.layout);
  gr::create_clouds(d, f.pipelines, f.clouds, f.layout);
  gr::create_far_land(d, f.pipelines, f.land, f.layout);
  gr::BloomSettings bs;
  bs.strength = 0.04;
  gr::create_bloom_pipelines(d, f.pipelines, f.bloom, bs);
  gr::PostSettings ps;
  ps.bloom = 0.04;
  gr::create_post(d, f.pipelines, f.post, f.layout, wgpu::TextureFormat::RGBA8Unorm, ps);
  if (!f.pipelines.finish(d)) {
    lab.fail("the far land's pipelines did not build",
             "Read the gpu: lines above (engine/render/far_land.wgsl). Skill: render.");
    return;
  }
  ge::gpu::end_load(d);
  f.frame = gr::frame_group(d, f.layout, f.uniforms, f.sky);
  gr::bind_cloud_targets(d, f.clouds, f.targets);
  const wgpu::TextureView inputs[2] = {f.targets.hdr_view, f.targets.hdr_view};
  gr::create_bloom_targets(d, f.bloom, w, h, inputs);
  f.post_group = gr::post_group(d, f.post, f.targets.hdr_view, f.bloom.view);
  f.rec.init(d, "far-land.timer", 16);
  f.out = ge::gpu::create_target(d, "far-land.final", w, h);

  for (size_t i = 0; i < kinds.size(); ++i) {
    const gr::FarLandKind& k = *kinds[i];
    const gr::FarMesh& mesh = meshes[i];
    d.loading = true;
    gr::set_far_land(d, f.land, k, site, mesh);
    ge::gpu::end_load(d);
    const double heading = lab.arg("heading").empty() ? highest_bearing(mesh, k.near_m)
                                                      : lab.arg_num("heading") * kDeg;
    const double snow = gr::far_snow_line(k, mesh, 0);
    const std::vector<std::pair<std::string, std::string>> inputs_of = {
        {"kind", k.name},
        {"sky", k.sky},
        {"eye", fixed(kEyeUp, 0) + " m over the floor"},
        {"heading", fixed(heading / kDeg, 0) + " deg"}};
    for (const Light& l : kLights) {
      const std::string id = std::string(k.name) + "-" + l.id;
      if (!lab.wants(id)) continue;
      const gr::FrameView v = view_of(k, l, heading, site.floor_y);
      std::string title = std::string(k.name) + " " + l.title;
      for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      gl::Shot shot{id, title,
                    std::string(k.what) + "; the snow line at " + fixed(snow, 0) +
                        " m (the valley's " + fixed(k.temp_c, 0) + " C cooled 6.5 K a km).",
                    "ranges to the horizon on the Earth's curve, fading into the far air; snow "
                    "only above the snow line and off the steepest rock; water mirroring the sky; "
                    "no seam at the clouds' base",
                    inputs_of};
      shot.inputs.push_back({"sun", fixed(l.sun_el, 0) + " deg"});
      lab.shot(shot, draw(d, f, v, 0), Det::measured);
    }
    if (i != 0) continue;
    for (double warm : warms) {
      const std::string id = std::string(k.name) + "-warm" + fixed(warm, 0);
      if (!lab.wants(id)) continue;
      const gr::FrameView v = view_of(k, kLights[0], heading, site.floor_y);
      const double line = gr::far_snow_line(k, mesh, warm);
      gl::Shot shot{id, std::string("WARMED ") + fixed(warm, 0) + " C",
                    std::string(k.name) + " with its valley " + fixed(k.temp_c + warm, 0) +
                        " C: the snow line at " + fixed(line, 0) + " m.",
                    "colder: the snow down the ranges and the lake frozen under -3 C; warmer: "
                    "only the highest crests white",
                    inputs_of};
      lab.shot(shot, draw(d, f, v, warm), Det::measured);
    }
  }
  ls::checked(lab, static_cast<int>(d.errors.load()), static_cast<int>(d.pipelines_after_load));
}

const gl::LabSpec kSpec{
    "far-land",
    gl::LabKind::picture,
    "engine",
    "engine/render/far_land.hpp",
    "render",
    "Does each far land read as a real (or an alien) landscape seen from high up: ranges to the "
    "horizon, the snow where the air freezes, water, grass, forest and rock where they belong, "
    "the ranges' shadows at a low sun and the far air's haze?",
    {{"kinds", "all", "the kinds drawn, by name, or all"},
     {"warm", "-14,10", "the first kind's extra views: its valley warmed by each, C"},
     {"heading", "", "the eye's heading, deg (empty: where the ranges stand highest)"},
     {"detail", "1", "the mesh's density against the game's (bearings and rings)"},
     {"width", "640", "a view's width, px (16:9)"}},
    "--kinds=alps,ember --warm=-14 --detail=0.25 --width=192",
    run};
GE_LAB(kSpec);

}  // namespace
