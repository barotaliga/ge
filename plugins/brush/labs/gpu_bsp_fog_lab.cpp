// LAB `gpu-bsp-fog`: A CLASSIC MAP'S OWN AREAS OF FOG (plugins/brush/app/bsp_fog.hpp) LOOKED AT:
// the volumes the map places (`fog_volume` entities) and the hazes over its liquids, read when it
// loads, drawn by the v2 renderer through the engine's area fog (render/fog.hpp), lit by the sun
// past the renderer's own sun map. Each view is drawn with the fog and without it (`--fog=0`
// draws every view without), side by side.
//
// Without `--pak` the brush plugin's fog fixture: a hall filled with dust by a `fog_volume`
// entity, two windows and a door onto an open yard the sun stands over, and a store behind it
// with a pit in its floor the lab lays a pool of lava in (the fixture's compile writes no liquid
// contents) and a pond sunk in the yard it fills with water. With `--pak=<file>` and
// `--maps=<a>,<b>` a pack's maps from their starts (and `--view`s), each with the hazes over its
// own lava and slime.
//
// THE AIR'S FOG over the pools (M51c, `--air=1`): an evening's air handed to the fog (`--mean`,
// `--wind`, `--water`, `--hour`: by default a classic level's, 2 C in a 9 m/s wind under low cloud
// over pools at 16 C), so each pool of water and slime steams in wisps rising off it; the sun
// stays the lab's (`--sun`). `--air=0` asks no air (the before).
//
// ge:summary Lab: a BSP map's own fog: a hall's dust lit through its windows, the haze over lava
// ge:keywords lab bsp fog fog-volume dust lava-haze slime-haze shafts window sky-share
// ge:keywords before-after gpu-bsp-fog steam-fog pond pool_mist
// ge:skill brush render labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_fog.hpp"
#include "brush/app/bsp_liquids.hpp"
#include "brush/app/bsp_sky.hpp"
#include "brush/app/draw_data.hpp"
#include "brush/write.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "render/bloom.hpp"
#include "render/clouds.hpp"
#include "render/fog.hpp"
#include "render/frame.hpp"
#include "render/liquid.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "render/water.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace ba = ge::brush::app;
namespace ls = ge::lab::liquid_scene;
using gl::Det;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

struct View {
  std::string id, title, caption;
  size_t map = 0;
  double eye[3] = {0, 0, 0};
  double heading = 0, pitch = 0;
};

struct MapState {
  ba::Scene scene;
  ba::BspDraw bsp;
  ba::BspFogShadow shadow;
  gr::Water water;
  gr::Liquids liquids;
  ba::LiquidWorld world;
  ba::LiquidRows rows;
  ba::SkyPick sky;
  std::vector<gr::FogVolume> fog;
};

struct Frame {
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms;
  gr::FrameUniforms staged{};
  gr::FrameHistory history;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame, post_group;
  gr::Sky sky;
  gr::CloudPass clouds;
  gr::Fog fog;
  gr::Bloom bloom;
  gr::Post post;
  gr::FrameRecorder rec;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

// One picture after `frames` frames (the fog's history settling), with the map's volumes or none.
gl::Picture draw(ge::gpu::Device& d, Frame& f, MapState& m, gr::FrameView v, bool fog, int frames) {
  ba::Options o;
  o.sky_light = m.sky.sky_light;
  o.sky_seen = m.sky.sky_seen;
  for (int j = 0; j < 3; ++j) o.sky_tint[j] = m.sky.tint[j];
  o.fog_sun = true;
  const std::vector<gr::FogVolume> none;
  gl::Picture pic;
  for (int k = 0; k < frames; ++k) {
    f.rec.begin_frame(d);
    gr::fill_uniforms(f.staged, v, f.w, f.h, &f.history);
    gr::prepare_fog(d, f.fog, v, fog ? m.fog : none, k == 0);
    gr::fog_uniforms(f.fog, f.staged);
    d.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
    gr::prepare_sky(f.sky, v);
    ba::prepare_bsp(d, m.bsp, v, o);
    ba::prepare_bsp_fog_shadow(d, m.shadow, m.bsp, v);
    gr::bind_fog_shadows(f.fog, m.shadow.group);
    ba::light_rooms(m.water, m.liquids, m.rows, o.minlight, o.brightness);
    gr::prepare_water(d, m.water, v);
    gr::prepare_liquid(d, m.liquids, v);
    wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
    gr::encode_sky_tables(f.sky, f.pipelines, f.rec, e);
    // The fog before the map: its faces read the volume. The sun's map the fog reads is the one
    // the BSP pass keeps (drawn on the first frame, read from the next).
    gr::encode_fog(f.fog, f.pipelines, f.rec, e, f.frame);
    ba::encode_bsp(m.bsp, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_sky(f.sky, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_clouds(f.clouds, f.pipelines, f.rec, e, f.targets, f.frame, v, f.staged);
    ba::encode_bsp_veil(m.bsp, f.pipelines, f.rec, e, f.targets);
    gr::encode_water(m.water, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_liquid(m.liquids, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_bloom(f.bloom, f.pipelines, f.rec, e, 0);
    gr::encode_post(f.post, f.pipelines, f.rec, e, f.frame, f.post_group, f.out.view);
    f.rec.end_frame(e);
    wgpu::CommandBuffer cb = e.Finish();
    d.queue.Submit(1, &cb);
    f.rec.after_submit(d);
    v.time_s += 1.0 / 30;
    if (k + 1 == frames) {
      const ge::gpu::Image img = ge::gpu::read_now(d, f.out);
      pic = gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
    }
  }
  return pic;
}

std::vector<std::string> split(const std::string& s, char c) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    size_t end = s.find(c, at);
    if (end == std::string::npos) end = s.size();
    if (end > at) out.push_back(s.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

bool parse_view(const std::string& spec, const std::vector<std::string>& names, View* out) {
  const size_t colon = spec.find(':');
  if (colon == std::string::npos) return false;
  double x, y, z, hd, pt;
  if (std::sscanf(spec.c_str() + colon + 1, "%lf,%lf,%lf,%lf,%lf", &x, &y, &z, &hd, &pt) != 5)
    return false;
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i] != spec.substr(0, colon)) continue;
    *out =
        View{names[i] + "-at-" + ls::fixed(x, 0) + "-" + ls::fixed(z, 0),
             names[i] + " at " + ls::fixed(x, 1) + ", " + ls::fixed(y, 1) + ", " + ls::fixed(z, 1),
             "A view picked by hand.",
             i,
             {x, y, z},
             hd * kDeg,
             pt * kDeg};
    return true;
  }
  return false;
}

// The fixture's pool: the pit sunk in the store's floor (plugins/brush/fixture.cpp's fog_fixture,
// a unit 0.025 m), its top 0.25 m under the floor.
ba::LiquidPatch fixture_pool() {
  ba::LiquidPatch p;
  p.kind = ba::kLava;
  p.level = -0.25;
  p.box[0] = 224 * 0.025, p.box[1] = -608 * 0.025, p.box[2] = 416 * 0.025, p.box[3] = -464 * 0.025;
  p.area = (p.box[2] - p.box[0]) * (p.box[3] - p.box[1]);
  p.centre[0] = (p.box[0] + p.box[2]) / 2;
  p.centre[1] = (p.box[1] + p.box[3]) / 2;
  p.largest = p.area;
  return p;
}

// The fixture's pond: sunk in the yard's flagstones, its water 0.1 m under them.
ba::LiquidPatch fixture_pond() {
  ba::LiquidPatch p;
  p.kind = ba::kWater;
  p.level = -0.1;
  p.box[0] = 192 * 0.025, p.box[1] = 560 * 0.025, p.box[2] = 448 * 0.025, p.box[3] = 880 * 0.025;
  p.area = (p.box[2] - p.box[0]) * (p.box[3] - p.box[1]);
  p.centre[0] = (p.box[0] + p.box[2]) / 2;
  p.centre[1] = (p.box[1] + p.box[3]) / 2;
  p.largest = p.area;
  return p;
}

void run(gl::Lab& lab) {
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 9 / 16;
  const int frames = std::max(1, int(lab.arg_num("frames")));
  const bool fog_on = lab.arg_num("fog") != 0;

  // ── The maps ──
  ba::Pack pack;
  std::vector<uint8_t> palette;
  std::vector<std::string> paths, names;
  const bool fixture = lab.arg("pak").empty();
  if (!fixture) {
    std::string err;
    if (!ba::read_pack(lab.arg("pak"), &pack, &err)) {
      lab.fail(err,
               "Pass --pak=<a pack file> holding maps/*.bsp and gfx/palette.lmp. Skill: brush.");
      return;
    }
    const auto pal = ba::pack_file(pack, "gfx/palette.lmp");
    palette.assign(pal.begin(), pal.end());
    for (const std::string& m : split(lab.arg("maps"), ',')) {
      paths.push_back("maps/" + m + ".bsp");
      names.push_back(m);
    }
  } else {
    std::vector<uint8_t> bytes;
    std::string err;
    if (!ge::brush::write_bsp(ge::brush::fog_fixture(), ge::brush::Family::v29, &bytes, &err)) {
      lab.fail("the fog fixture did not compile: " + err,
               "next: read plugins/brush/fixture.cpp's fog_fixture. Skill: brush.");
      return;
    }
    pack.bytes = bytes;
    pack.files.push_back({"maps/fog.bsp", 0, uint32_t(bytes.size())});
    paths.push_back("maps/fog.bsp");
    names.push_back("fog");
  }
  if (palette.size() < 768) {
    // The fixture's textures are palette greys from 16 up; lifted, so its rooms read.
    palette.resize(768);
    for (int i = 0; i < 256; ++i)
      for (int k = 0; k < 3; ++k)
        palette[size_t(i) * 3 + size_t(k)] = uint8_t(std::min(250, 90 + i * 3 / 4));
  }
  // The evening's air the pools are asked under.
  ge::wind::Climate air;
  air.mean_c = lab.arg_num("mean");
  air.wind = lab.arg_num("wind");
  air.cloud = 0.9;
  air.humidity = 0.85;
  air.water_c = lab.arg_num("water");
  ba::FogRows rows;
  if (lab.arg_num("air") != 0) rows.air = &air;
  rows.hour = lab.arg_num("hour");
  std::vector<std::unique_ptr<MapState>> maps;
  gl::LabTable table{
      "The volumes each map's fog is read into",
      {"map", "volume", "kind", "centre m", "size m", "density 1/m", "glow cd/m2/m", "sky"},
      {}};
  for (size_t i = 0; i < paths.size(); ++i) {
    auto m = std::make_unique<MapState>();
    std::string err;
    if (!ba::load_scene(pack, paths[i], palette, &m->scene, &err)) {
      lab.fail(paths[i] + ": " + err, "Check the map's name in the pack (--maps). Skill: brush.");
      return;
    }
    if (fixture) {
      m->scene.liquids.push_back(fixture_pool());
      m->scene.liquids.push_back(fixture_pond());
    }
    m->sky = ba::pick_sky(m->scene.sky_colours);
    m->fog = ba::bsp_fog(m->scene, rows);
    int placed = 0;
    for (const ge::brush::Entity& e : m->scene.map.entities)
      if (e.get("classname") == "fog_volume") ++placed;
    for (size_t k = 0; k < m->fog.size(); ++k) {
      const gr::FogVolume& v = m->fog[k];
      const char* kind = int(k) < placed         ? "placed"
                         : v.glow[0] > v.glow[1] ? "lava haze"
                         : v.glow[1] > 0         ? "slime haze"
                                                 : "the air's";
      table.rows.push_back(
          {names[i], std::to_string(k), kind,
           ls::fixed(v.centre[0], 1) + "," + ls::fixed(v.centre[1], 1) + "," +
               ls::fixed(v.centre[2], 1),
           ls::fixed(2 * v.half[0], 1) + " x " + ls::fixed(2 * v.half[1], 1) + " x " +
               ls::fixed(2 * v.half[2], 1),
           ls::fixed(v.density, 3),
           ls::fixed(v.glow[0], 1) + " " + ls::fixed(v.glow[1], 1) + " " + ls::fixed(v.glow[2], 1),
           ls::fixed(v.sky, 2)});
    }
    lab.metric(names[i] + ".volumes", double(m->fog.size()), "count");
    if (fixture) {
      lab.metric("fog.placed", placed, "count");
      if (!m->fog.empty()) lab.metric("fog.hall-sky", m->fog[0].sky, "share");
      if (m->fog.size() < 2)
        lab.fail("the fixture's fog read " + std::to_string(m->fog.size()) +
                     " volume(s), not the hall's dust and the pool's haze",
                 "next: read bsp_fog.cpp's fog_volume_of and the fixture's entity. Skill: brush.");
    }
    maps.push_back(std::move(m));
  }
  lab.table(table);

  // ── The views ──
  std::vector<View> views;
  if (fixture) {
    views.push_back({"fog-hall",
                     "The hall's dust, toward its windows",
                     "Inside the dusty hall looking out through its two windows and its door to "
                     "the sunny yard: the sun's shafts through them in the dust.",
                     0,
                     {8, 1.6, -9},
                     0,
                     2 * kDeg});
    views.push_back({"fog-hall-side",
                     "The hall's dust, across the shafts",
                     "From the hall's west end looking along it: the shafts from the windows "
                     "cross the dust to the floor, the rest of the hall in the dust's shade.",
                     0,
                     {1.2, 2.0, -6},
                     90 * kDeg,
                     -8 * kDeg});
    views.push_back({"fog-pit",
                     "The store's pit of lava",
                     "In the closed store behind the hall: the haze over the pool of lava glows "
                     "and flickers in the dark.",
                     0,
                     {11.4, 1.7, -10.8},
                     233 * kDeg,
                     -18 * kDeg});
    views.push_back({"fog-pond",
                     "The yard's pond on a cold, windy evening",
                     "Out in the yard, low by its pond, looking across it to the wall: the water "
                     "far warmer than the evening's air steams, wisps rising off it and leaning on "
                     "the wind.",
                     0,
                     {3, 1.2, 18},
                     90 * kDeg,
                     -6 * kDeg});
  } else {
    for (size_t i = 0; i < maps.size(); ++i) {
      const ba::Scene& s = maps[i]->scene;
      views.push_back({names[i] + "-start",
                       names[i] + ": the start",
                       "From where the player starts, looking the way they face.",
                       i,
                       {s.start[0], s.start[1], s.start[2]},
                       s.start_heading,
                       0});
    }
  }
  for (const std::string& spec : split(lab.arg("view"), ';')) {
    View v;
    if (parse_view(spec, names, &v))
      views.push_back(v);
    else
      lab.fail("--view=" + spec + " is not <map>:x,y,z,heading,pitch with the map in --maps",
               "next: write it as e.g. --view=start:20,-1,30,180,-15 and name its map in --maps.");
  }

  auto device = ge::gpu::create_device({.label = "gpu-bsp-fog", .validation = 1});
  if (!device) {
    if (ls::gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: no pictures.");
    for (const View& v : views) ls::placeholders(lab, {v.id.c_str()});
    ls::checked(lab, 0, 0);
    return;
  }
  ge::gpu::Device& d = *device;
  Frame f;
  f.w = w;
  f.h = h;
  f.targets = gr::create_targets(d, "gpu-bsp-fog", w, h);
  f.uniforms = gr::make_buffer(d, "gpu-bsp-fog.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  f.layout = gr::frame_layout(d);
  gr::create_sky(d, f.pipelines, f.sky, f.uniforms, f.layout);
  gr::create_clouds(d, f.pipelines, f.clouds, f.layout);
  gr::FogSettings fs;
  fs.range = lab.arg_num("range");
  gr::create_fog(d, f.pipelines, f.fog, f.layout, nullptr, fs);
  gr::BloomSettings bs;
  bs.strength = 0.05;
  gr::create_bloom_pipelines(d, f.pipelines, f.bloom, bs);
  gr::PostSettings ps;
  ps.bloom = 0.05;
  gr::create_post(d, f.pipelines, f.post, f.layout, wgpu::TextureFormat::RGBA8Unorm, ps);
  for (auto& m : maps) {
    ba::create_bsp(d, f.pipelines, m->bsp, m->scene, f.layout);
    ba::create_bsp_fog_shadow(d, m->shadow, m->bsp);
    gr::create_water(d, f.pipelines, m->water, f.layout);
    gr::create_liquid(d, f.pipelines, m->liquids, f.layout);
    m->world = ba::liquid_world(m->scene, m->rows);
    ba::add_bsp_liquids(d, m->water, m->liquids, m->world, m->rows);
  }
  if (!f.pipelines.finish(d)) {
    lab.fail("the BSP renderer's or the fog's pipelines did not build",
             "Read the gpu: lines above (plugins/brush/app/*.wgsl, engine/render/fog.wgsl). "
             "Skill: render.");
    return;
  }
  ge::gpu::end_load(d);
  f.frame = gr::frame_group(d, f.layout, f.uniforms, f.sky, nullptr, f.fog.volume_view);
  gr::bind_cloud_targets(d, f.clouds, f.targets);
  const wgpu::TextureView inputs[2] = {f.targets.hdr_view, f.targets.hdr_view};
  gr::create_bloom_targets(d, f.bloom, w, h, inputs);
  f.post_group = gr::post_group(d, f.post, f.targets.hdr_view, f.bloom.view);
  f.rec.init(d, "gpu-bsp-fog.timer", 32);
  f.out = ge::gpu::create_target(d, "gpu-bsp-fog.final", w, h);
  for (auto& m : maps) {
    ba::create_bsp_targets(d, m->bsp, f.targets);
    gr::create_water_targets(d, m->water, f.targets);
    gr::create_liquid_targets(d, m->liquids, f.targets);
  }

  // ── Each view with its fog and without, side by side ──
  for (const View& v : views) {
    if (!lab.arg("only").empty() && v.id.find(lab.arg("only")) == std::string::npos) continue;
    MapState& m = *maps[v.map];
    gr::FrameView fv;
    for (int k = 0; k < 3; ++k) fv.eye[k] = v.eye[k];
    fv.heading = v.heading;
    fv.pitch = v.pitch;
    fv.style = m.sky.style;
    fv.sun_elevation = lab.arg_num("sun") * kDeg;
    fv.sun_azimuth = lab.arg_num("azimuth") * kDeg;
    fv.altitude_m = 100;
    fv.time_s = lab.arg_num("time");
    fv.fov_y = 1.25;
    fv.near_m = 0.05;
    const gl::Picture without = draw(d, f, m, fv, false, frames);
    const gl::Picture with = draw(d, f, m, fv, fog_on, frames);
    gl::Picture sheet = gl::create_picture(int(w) * 2 + 30, int(h) + 40, {250, 250, 248});
    gl::draw_text(sheet, 10, 6, "WITHOUT ITS FOG", {24, 26, 32});
    gl::draw_text(sheet, 20 + int(w), 6, fog_on ? "WITH ITS FOG" : "WITH ITS FOG (--fog=0: OFF)",
                  {24, 26, 32});
    ls::paste(sheet, without, 10, 20);
    ls::paste(sheet, with, 20 + int(w), 20);
    lab.metric(v.id + ".froxel-volumes", double(f.fog.count), "count", Det::measured);
    lab.shot({v.id,
              v.title,
              v.caption + " Left without the map's fog, right with it (" +
                  std::to_string(m.fog.size()) + " volumes read off the map).",
              "The dust is lit only where the sun's shafts and the sky reach it; the haze over "
              "the lava glows orange, thickest at the melt.",
              {{"eye", ls::fixed(v.eye[0], 1) + "," + ls::fixed(v.eye[1], 1) + "," +
                           ls::fixed(v.eye[2], 1)},
               {"heading", ls::fixed(v.heading / kDeg, 0)},
               {"pitch", ls::fixed(v.pitch / kDeg, 0)}}},
             sheet, Det::measured);
  }
  ls::checked(lab, int(d.errors.load()), int(d.pipelines_after_load));
}

const gl::LabSpec kSpec{
    "gpu-bsp-fog",
    gl::LabKind::picture,
    "plugin:brush",
    "plugins/brush/app/bsp_fog.hpp",
    "brush",
    "Does a classic map's own fog (the dust its fog_volume entities place, the haze over its lava "
    "and slime) show, lit by the sun only through its windows and glowing over its lava?",
    {{"width", "480", "a pane's width, px (16:9)"},
     {"pak", "", "a pack file holding maps/*.bsp and gfx/palette.lmp (none: the fog fixture)"},
     {"maps", "start", "the maps in the pack, comma separated, without maps/ and .bsp"},
     {"view", "", "more views: <map>:x,y,z,heading,pitch (m, degrees), separated by ;"},
     {"fog", "1", "0 draws the right pane without the fog too (a before)"},
     {"sun", "22", "the sun's elevation, deg"},
     {"azimuth", "0", "the sun's bearing, deg (a world heading: 0 the map's +y)"},
     {"time", "40", "the views' time, s"},
     {"range", "60", "the froxels' depth, m"},
     {"frames", "8", "the frames drawn before each picture (the fog's history settling)"},
     {"only", "", "draw only the views whose id holds this"},
     {"air", "1", "1: an evening's air asked over the pools (their steam); 0: none (the before)"},
     {"mean", "2", "the air's day: its mean temperature, C"},
     {"wind", "9", "the air's wind, m/s"},
     {"water", "16", "the pools' water, C"},
     {"hour", "21", "the solar hour the air is asked at"}},
    "--width=128 --frames=2",
    run};
GE_LAB(kSpec);

}  // namespace
