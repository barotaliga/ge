// LAB `se-world`: SHOOTER'S EDGE'S WORLD PASS LOOKED AT. A free map drawn from named views in the
// game's two looks side by side (games/shooters_edge/app/look.hpp): CLASSIC (crisp texels, the
// lightmaps and their styles, the map's sky, the palette's colours) and MODERN (filtered, real-time
// lights with shadows, our sky, bloom). Then what moves: a light style played over a second, a
// texture's animation, the effects (an explosion, a rocket's trail, a hit, a teleport, lava thrown
// up, with their dynamic lights) in both looks, the screen's blends, and the sky a switch
// (ce_skies: our sky, or the pack's under the modern look). Every view is drawn culled
// (the eye's potentially visible set, then two-phase Hi-Z; brush/app/bsp_cull.hpp); and the GPU's
// half of the cull (the frustum and the pyramid, the set off) must draw the same pixels as drawing
// everything. The set is the map's own data and not held so: the arena's rooms see two doorways
// deep, so a face three aligned doorways off is dropped by it, as a map with a coarse vis does.
//
// Without `--pak` the map is the lab's own arena (the brush plugin's sample fixture with a light
// that flickers and a wall that animates), so CI needs no game data; with `--pak=<file>` and
// `--maps=<a>,<b>` the maps come out of a pack, for private pictures only. `--cvars="r_lava_glow
// 0"` sets the picture's r_ cvars (look.hpp) as the console would.
//
// ge:summary Lab: Shooter's Edge's world in both looks, its light styles, effects, blends and cull
// ge:keywords lab se-world shooter world classic modern look light-style animated-texture effects
// ge:keywords explosion trail dynamic-light screen-blend cull pvs hi-z palette
// ge:skill shooter render labs
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_sky.hpp"
#include "brush/app/draw_data.hpp"
#include "brush/write.hpp"
#include "console/console.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "render/bloom.hpp"
#include "render/clouds.hpp"
#include "render/frame.hpp"
#include "render/particles.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"
#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/app/look.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace ba = ge::brush::app;
namespace br = ge::brush;
namespace ls = ge::lab::liquid_scene;
namespace fx = shooters_edge::effects;
namespace lk = shooters_edge::look;
using gl::Det;

// The arena's light styles: our own patterns (draw_data.hpp's letters, 'm' the light as baked).
constexpr int kFlicker = 1;
constexpr const char* kFlickerPattern = "mmmaammmzmmmamzmmaamm";

struct Frame {
  ge::gpu::PipelineSet pipelines;
  gr::FrameTargets targets;
  wgpu::Buffer uniforms;
  gr::FrameUniforms staged{};
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup frame;
  gr::Sky sky;
  gr::CloudPass clouds;
  gr::Bloom bloom;
  gr::Post post[2];  // classic, modern
  wgpu::BindGroup post_group[2];
  gr::Particles particles;
  gr::FrameRecorder rec;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
  lk::Picture picture;  // the r_ cvars, as --cvars set them
};

struct MapState {
  std::string name;
  ba::Scene scene;
  ba::BspDraw bsp;
  ba::SkyPick sky;
};

struct View {
  std::string id, title, caption;
  double eye[3] = {0, 0, 0};
  double heading = 0, pitch = 0;
};

// The arena: the sample fixture with the hall's lamp flickering (a style of its own), the side
// room's walls a two-frame animation and the vault open to the sky.
br::Fixture arena() {
  br::Fixture f = br::sample_fixture();
  if (!f.lights.empty()) f.lights[0].style = kFlicker;
  if (f.rooms.size() > 1) f.rooms[1].wall = "+0glow";
  if (f.rooms.size() > 2) f.rooms[2].wall = "+1glow";
  for (br::FixtureRoom& r : f.rooms)
    if (r.name == "vault") r.ceiling = "sky1";
  return f;
}

// A named view a room: from near its low corner, looking across it to the far one.
std::vector<View> room_views(const br::Fixture& f, const std::string& map) {
  std::vector<View> out;
  for (const br::FixtureRoom& r : f.rooms) {
    float a[3], b[3];
    for (int k = 0; k < 3; ++k) {
      a[k] = float(r.min[k]) + 0.15f * float(r.max[k] - r.min[k]);
      b[k] = float(r.max[k]) - 0.15f * float(r.max[k] - r.min[k]);
    }
    a[2] = float(r.min[2]) + 56;  // eye height, map units
    b[2] = float(r.min[2]) + 24;
    double e[3], t[3];
    br::to_engine(a, e);
    br::to_engine(b, t);
    View v;
    v.id = map + "-" + r.name;
    std::replace(v.id.begin(), v.id.end(), ' ', '-');
    v.title = map + ": the " + r.name;
    v.caption = "From near the " + r.name + "'s corner, looking across it.";
    for (int k = 0; k < 3; ++k) v.eye[k] = e[k];
    const double dx = t[0] - e[0], dz = t[2] - e[2];
    v.heading = ge::core::libm::atan2(dx, dz);
    v.pitch = ge::core::libm::atan2(t[1] - e[1], std::sqrt(dx * dx + dz * dz));
    out.push_back(v);
  }
  return out;
}

// A view up into the open sky: under the level sky face nearest the start (a ceiling of sky, at
// most 4 m under it), looking up at it; `fallback` pitched up when the map has none.
View sky_view(const ba::Scene& s, const View& fallback) {
  double best = 1e30;
  View v = fallback;
  v.pitch = std::max(v.pitch, 0.0) + 0.45;
  for (size_t i = 0; i + 2 < s.indices.size(); i += 3) {
    const ba::DrawVertex& a = s.vertices[s.indices[i]];
    if (a.kind != ba::kSky || std::fabs(a.nrm[1]) < 0.7) continue;
    double c[3] = {0, 0, 0};
    for (size_t k = 0; k < 3; ++k)
      for (int j = 0; j < 3; ++j) c[j] += s.vertices[s.indices[i + k]].pos[j] / 3.0;
    if (c[1] < s.start[1] + 1) continue;
    const double dx = c[0] - s.start[0], dy = c[1] - s.start[1], dz = c[2] - s.start[2];
    const double d = dx * dx + dy * dy + dz * dz;
    if (d >= best) continue;
    best = d;
    v.title = fallback.title.substr(0, fallback.title.find(':')) + ": under the open sky";
    v.eye[0] = c[0];
    v.eye[1] = std::max(s.start[1] + 0.6, c[1] - 4);
    v.eye[2] = c[2];
    v.pitch = 0.9;
  }
  return v;
}

gr::FrameView frame_view(const View& view, const MapState& m, double t) {
  gr::FrameView v;
  for (int k = 0; k < 3; ++k) v.eye[k] = view.eye[k];
  v.heading = view.heading;
  v.pitch = view.pitch;
  ba::dress_sky(v, m.sky, m.scene.maxs[1], t);
  v.altitude_m = 100;
  v.fov_y = 1.25;
  v.near_m = 0.05;
  v.clouds.quarter = false;
  v.clouds.spread = false;
  v.time_s = t;
  return v;
}

// One frame of `m` from `v` in `look`: the world, the sky (modern), the particles, bloom and post.
gl::Picture draw(ge::gpu::Device& d, Frame& f, MapState& m, const gr::FrameView& v, lk::Look look,
                 bool cull, double particle_dt = 0, const double* blend = nullptr,
                 bool pvs = true) {
  f.rec.begin_frame(d);
  gr::fill_uniforms(f.staged, v, f.w, f.h);
  d.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
  gr::prepare_sky(f.sky, v);
  ba::Options o = lk::world_options(look, {}, f.picture);
  o.cull = cull;
  o.pvs = pvs;
  o.sky_light = m.sky.sky_light;
  o.sky_seen = m.sky.sky_seen;
  for (int k = 0; k < 3; ++k) o.sky_tint[k] = m.sky.tint[k];
  ba::prepare_bsp(d, m.bsp, v, o);
  f.particles.settings.glow = float(lk::effects_glow(look, f.picture));
  gr::prepare_particles(d, f.particles, particle_dt, v.near_m);
  const int p = look == lk::Look::classic ? 0 : 1;
  const double none[4] = {0, 0, 0, 0};
  gr::set_post_blend(d, f.post[p], blend ? blend : none);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  gr::encode_particles_sim(f.particles, f.pipelines, f.rec, e);
  gr::encode_sky_tables(f.sky, f.pipelines, f.rec, e);
  ba::encode_bsp(m.bsp, f.pipelines, f.rec, e, f.targets, f.frame);
  gr::encode_sky(f.sky, f.pipelines, f.rec, e, f.targets, f.frame);
  gr::encode_clouds(f.clouds, f.pipelines, f.rec, e, f.targets, f.frame, v, f.staged);
  ba::encode_bsp_veil(m.bsp, f.pipelines, f.rec, e, f.targets);
  gr::encode_particles(f.particles, f.pipelines, f.rec, e, f.targets, f.frame);
  gr::encode_bloom(f.bloom, f.pipelines, f.rec, e, 0);
  gr::encode_post(f.post[p], f.pipelines, f.rec, e, f.frame, f.post_group[p], f.out.view);
  f.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  f.rec.after_submit(d);
  const ge::gpu::Image img = ge::gpu::read_now(d, f.out);
  return gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
}

// Pictures in a row on a sheet, each with its label over it.
gl::Picture row(const std::vector<gl::Picture>& pics, const std::vector<std::string>& labels) {
  const int n = int(pics.size());
  const int w = n > 0 ? pics[0].width : 1, h = n > 0 ? pics[0].height : 1;
  gl::Picture sheet = gl::create_picture(w * n + 10 * (n + 1), h + 30, {250, 250, 248});
  for (int i = 0; i < n; ++i) {
    const int ox = 10 + i * (w + 10);
    gl::draw_text(sheet, ox, 6, labels[size_t(i)], {24, 26, 32});
    ls::paste(sheet, pics[size_t(i)], ox, 20);
  }
  return sheet;
}

std::vector<std::string> split(const std::string& s, char c) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size() && !s.empty()) {
    size_t end = s.find(c, at);
    if (end == std::string::npos) end = s.size();
    if (end > at) out.push_back(s.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

// The pixels that differ at all between two pictures.
int differ(const gl::Picture& a, const gl::Picture& b) {
  int n = 0;
  for (size_t i = 0; i + 3 < a.rgba.size() && i + 3 < b.rgba.size(); i += 4)
    n +=
        (a.rgba[i] != b.rgba[i] || a.rgba[i + 1] != b.rgba[i + 1] || a.rgba[i + 2] != b.rgba[i + 2])
            ? 1
            : 0;
  return n;
}

void run(gl::Lab& lab) {
  const uint32_t w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 9 / 16;
  const double t = lab.arg_num("time");

  // ── The maps: the arena, or a pack's ──
  ba::Pack pack;
  std::vector<uint8_t> palette;
  std::vector<std::string> paths, names;
  br::Fixture fixture = arena();
  const bool own = lab.arg("pak").empty();
  if (!own) {
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
    if (!br::write_bsp(fixture, br::Family::v29, &bytes, &err)) {
      lab.fail("the arena did not write: " + err,
               "Read brush/fixture.cpp's refusal. Skill: brush.");
      return;
    }
    pack.bytes = bytes;
    pack.files.push_back({"maps/arena.bsp", 0, uint32_t(bytes.size())});
    paths.push_back("maps/arena.bsp");
    names.push_back("arena");
  }
  if (palette.size() < 768) {
    // The arena's palette: our own ramps, so the classic grade has colours to pull to.
    palette.resize(768);
    for (int i = 0; i < 256; ++i) {
      const int ramp = i / 16, v = (i % 16) * 16 + 8;
      const int tint[16][3] = {{1, 1, 1}, {3, 2, 1}, {2, 2, 3}, {1, 3, 1}, {3, 1, 1}, {3, 3, 1},
                               {1, 2, 3}, {3, 2, 2}, {2, 3, 2}, {2, 1, 3}, {3, 3, 2}, {2, 2, 2},
                               {1, 1, 3}, {3, 1, 2}, {2, 3, 3}, {3, 2, 3}};
      for (int k = 0; k < 3; ++k)
        palette[size_t(i) * 3 + size_t(k)] = uint8_t(std::min(255, v * tint[ramp][k] / 3));
    }
  }
  std::vector<std::unique_ptr<MapState>> maps;
  std::vector<View> views;
  for (size_t i = 0; i < paths.size(); ++i) {
    auto m = std::make_unique<MapState>();
    m->name = names[i];
    std::string err;
    if (!ba::load_scene(pack, paths[i], palette, &m->scene, &err)) {
      lab.fail(paths[i] + ": " + err, "Check the map's name in the pack (--maps). Skill: brush.");
      return;
    }
    const ba::Scene& s = m->scene;
    lab.metric(names[i] + ".light-layers", double(s.light_layers), "count");
    lab.metric(names[i] + ".animated-frames", double(s.frames.size()), "count");
    m->sky = ba::pick_sky(s.sky_colours);
    ba::night_over(m->sky, names[i]);
    if (own) {
      for (const View& v : room_views(fixture, names[i])) views.push_back(v);
    } else {
      View v;
      v.id = names[i] + "-start";
      v.title = names[i] + ": the start";
      v.caption = "From where the player starts, looking the way they face.";
      for (int k = 0; k < 3; ++k) v.eye[k] = s.start[k];
      v.eye[1] += 0.6;
      v.heading = s.start_heading;
      views.push_back(v);
    }
    maps.push_back(std::move(m));
  }
  const char* const kShots[] = {"styles", "animation", "effects", "blends", "skies"};

  auto device = ge::gpu::create_device({.label = "se-world", .validation = 1});
  if (!device) {
    if (ls::gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: no pictures.");
    for (const View& v : views) ls::placeholders(lab, {v.id.c_str()});
    for (const char* id : kShots) ls::placeholders(lab, {id});
    ls::checked(lab, 0, 0);
    return;
  }
  ge::gpu::Device& d = *device;
  Frame f;
  {
    ge::console::Console con;
    lk::add_cvars(con, f.picture);
    con.execute(lab.arg("cvars"));
    for (const auto& p : con.problems()) lab.note("--cvars: " + p.what);
  }
  f.w = w;
  f.h = h;
  f.targets = gr::create_targets(d, "se-world", w, h);
  f.uniforms = gr::make_buffer(d, "se-world.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  f.layout = gr::frame_layout(d);
  gr::create_sky(d, f.pipelines, f.sky, f.uniforms, f.layout);
  gr::create_clouds(d, f.pipelines, f.clouds, f.layout);
  gr::BloomSettings bs;
  bs.strength = lk::post_settings(lk::Look::modern, f.picture).bloom;
  gr::create_bloom_pipelines(d, f.pipelines, f.bloom, bs);
  for (int p = 0; p < 2; ++p) {
    const lk::Look look = p == 0 ? lk::Look::classic : lk::Look::modern;
    gr::create_post(d, f.pipelines, f.post[p], f.layout, wgpu::TextureFormat::RGBA8Unorm,
                    lk::post_settings(look, f.picture));
  }
  gr::set_grade(d, f.post[0], lk::palette_grade(palette));
  gr::create_particles(d, f.pipelines, f.particles, f.layout);
  for (auto& m : maps) {
    ba::create_bsp(d, f.pipelines, m->bsp, m->scene, f.layout);
    ba::set_light_style(m->bsp, kFlicker, kFlickerPattern);
  }
  if (!f.pipelines.finish(d)) {
    lab.fail("the world pass's pipelines did not build",
             "Read the gpu: lines above (plugins/brush/app/*.wgsl, engine/render). Skill: render.");
    return;
  }
  ge::gpu::end_load(d);
  f.frame = gr::frame_group(d, f.layout, f.uniforms, f.sky);
  gr::bind_cloud_targets(d, f.clouds, f.targets);
  const wgpu::TextureView inputs[2] = {f.targets.hdr_view, f.targets.hdr_view};
  gr::create_bloom_targets(d, f.bloom, w, h, inputs);
  for (int p = 0; p < 2; ++p)
    f.post_group[p] = gr::post_group(d, f.post[p], f.targets.hdr_view, f.bloom.view);
  gr::bind_particle_targets(d, f.particles, f.targets);
  f.rec.init(d, "se-world.timer", 24);
  f.out = ge::gpu::create_target(d, "se-world.final", w, h);
  for (auto& m : maps) ba::create_bsp_targets(d, m->bsp, f.targets);
  MapState& first = *maps[0];

  // ── The named views, both looks; each culled and unculled, which must agree ──
  // A first frame of each look settles what a frame carries to the next (the sky's tables).
  for (lk::Look look : {lk::Look::classic, lk::Look::modern})
    draw(d, f, first, frame_view(views[0], first, t), look, false);
  int disagree = 0;
  for (const View& view : views) {
    MapState* mp = maps[0].get();
    for (auto& mm : maps)
      if (view.id.rfind(mm->name + "-", 0) == 0) mp = mm.get();
    MapState& m = *mp;
    const gr::FrameView fv = frame_view(view, m, t);
    std::vector<gl::Picture> pics;
    for (lk::Look look : {lk::Look::classic, lk::Look::modern}) {
      pics.push_back(draw(d, f, m, fv, look, true));
      const ba::CullStats cs = ba::read_bsp_cull(d, m.bsp.cull);
      // The GPU's half held exact: the frustum and the pyramid alone against everything (the
      // set is the map's own data: the arena's rooms see two doorways deep, no further).
      const gl::Picture gpu = draw(d, f, m, fv, look, true, 0, nullptr, false);
      const ba::CullStats gs = ba::read_bsp_cull(d, m.bsp.cull);
      const gl::Picture all = draw(d, f, m, fv, look, false);
      const int n = differ(gpu, all);
      disagree += n;
      const std::string key = view.id + "." + lk::look_name(look);
      lab.metric(key + ".faces", double(cs.chunks), "count", Det::measured);
      lab.metric(key + ".faces.pvs-culled", double(cs.pvs), "count", Det::measured);
      lab.metric(key + ".faces.drawn", double(cs.early + cs.late), "count", Det::measured);
      lab.metric(key + ".faces.frustum-culled", double(cs.frustum), "count", Det::measured);
      lab.metric(key + ".faces.hidden", double(cs.hidden), "count", Det::measured);
      lab.metric(key + ".faces.late", double(cs.late), "count", Det::measured);
      lab.metric(key + ".faces.drawn-without-pvs", double(gs.early + gs.late), "count",
                 Det::measured);
      lab.metric(key + ".cull-differs", double(n), "px", Det::measured);
      if (n > 0) {
        // Where they differ, red over the unculled frame, to read the face the cull lost.
        gl::Picture where = all;
        for (size_t i = 0; i + 3 < where.rgba.size(); i += 4) {
          if (std::memcmp(&where.rgba[i], &gpu.rgba[i], 3) != 0)
            where.rgba[i] = 255, where.rgba[i + 1] = 0, where.rgba[i + 2] = 0;
        }
        lab.shot({view.id + "-" + lk::look_name(look) + "-cull",
                  view.title + ": the cull's miss",
                  "Culled, drawn whole, and the pixels that differ in red.",
                  "Nothing red: the cull should draw what drawing everything draws.",
                  {}},
                 row({gpu, all, where}, {"CULLED", "EVERYTHING", "DIFFERENT"}), Det::measured);
      }
    }
    lab.shot({view.id,
              view.title,
              view.caption + " Left the classic look (crisp texels, the lightmaps, the palette's "
                             "colours), right the modern (filtered, real-time lights, bloom).",
              "Both looks show the same room; the classic's texels are squares, the modern's "
              "light pools round its lamps.",
              {{"eye", ls::fixed(view.eye[0], 1) + "," + ls::fixed(view.eye[1], 1) + "," +
                           ls::fixed(view.eye[2], 1)}}},
             row(pics, {"CLASSIC", "MODERN"}), Det::measured);
  }
  lab.metric("cull-differs", double(disagree), "px", Det::measured);
  if (disagree != 0)
    lab.fail(std::to_string(disagree) + " pixels differ between the culled and unculled frames",
             "The cull dropped a face it should draw: read bsp_cull.wgsl's tests. Skill: render.");

  // ── A light style played: the flickering lamp's room at five moments, classic ──
  const View& hall = views[0];
  {
    std::vector<gl::Picture> pics;
    std::vector<std::string> labels;
    for (int k = 0; k < 5; ++k) {
      const double at = t + 0.1 * k;
      pics.push_back(draw(d, f, first, frame_view(hall, first, at), lk::Look::classic, true));
      labels.push_back(ls::fixed(at, 1) + " S: '" +
                       std::string(1, kFlickerPattern[size_t(std::floor(at * ba::kStyleRate)) %
                                                      std::string(kFlickerPattern).size()]) +
                       "'");
    }
    lab.shot({"styles",
              "a light style played",
              "The arena's hall in the classic look a tenth of a second apart: its lamp's light is "
              "a style of its own, played as a pattern of letters ('m' the light as baked).",
              "The hall's light steps with the letters; the ambient stays.",
              {{"pattern", kFlickerPattern}}},
             row(pics, labels), Det::measured);
  }
  // ── A texture's animation: the side room's walls at two frames ──
  {
    const View& side = views.size() > 1 ? views[1] : views[0];
    std::vector<gl::Picture> pics;
    for (double at : {t, t + 1 / ba::kFrameRate})
      pics.push_back(draw(d, f, first, frame_view(side, first, at), lk::Look::classic, true));
    lab.shot({"animation",
              "an animated texture",
              "The side room's walls are a two-frame texture (+0 and +1 of one name), a fifth of "
              "a second apart.",
              "The walls' pattern changes between the frames; the floor does not.",
              {}},
             row(pics, {"FRAME AT " + ls::fixed(t, 1) + " S", "A FIFTH OF A SECOND LATER"}),
             Det::measured);
  }
  // ── The effects, both looks ──
  {
    gr::FrameView fv = frame_view(hall, first, t);
    const double s = ge::core::libm::sin(hall.heading), c = ge::core::libm::cos(hall.heading);
    const auto ahead = [&](double fwd, double right, double up, double out[3]) {
      out[0] = hall.eye[0] + s * fwd + c * right;
      out[1] = hall.eye[1] + up;
      out[2] = hall.eye[2] + c * fwd - s * right;
    };
    double boom[3], from[3], to[3], hit[3], tele[3], lava[3];
    ahead(5, -1.2, -0.2, boom);
    ahead(1.5, 0.8, -0.3, from);
    ahead(6, 1.6, 0.2, to);
    ahead(4, 2.2, -0.8, hit);
    ahead(6, -2.6, -1.5, tele);
    ahead(3.5, 0.2, -1.4, lava);
    const double wall[3] = {-c, 0, s};
    std::vector<gl::Picture> pics;
    for (lk::Look look : {lk::Look::classic, lk::Look::modern}) {
      fx::Particles births;
      fx::explosion(boom, 1, &births);
      fx::rocket_trail(from, to, 2, &births);
      fx::impact(hit, wall, 3, &births);
      fx::teleport_splash(tele, 4, &births);
      fx::lava_splash(lava, 5, &births);
      gr::clear_particles(d, f.particles);
      gr::emit_particles(f.particles, births);
      ba::DynamicLight lights[2];
      fx::explosion_light(boom, 0.12, &lights[0]);
      lights[1] = fx::rocket_light(to);
      ba::set_dynamic_lights(first.bsp, lights);
      draw(d, f, first, fv, look, true, 0.12);  // the births flown a moment
      pics.push_back(draw(d, f, first, fv, look, true, 0));
      lab.metric(std::string("effects.") + lk::look_name(look) + ".births", double(births.size()),
                 "count", Det::measured);
    }
    ba::set_dynamic_lights(first.bsp, {});
    gr::clear_particles(d, f.particles);
    lab.shot({"effects",
              "the effects",
              "An explosion (sparks, smoke and its fading light), a rocket's trail and its glow, a "
              "shot's sparks off a wall, a teleport's column of light and lava thrown up, a moment "
              "after they began, in the classic look and the modern.",
              "The sparks glow and streak; the explosion and the rocket light the walls near them.",
              {}},
             row(pics, {"CLASSIC", "MODERN"}), Det::measured);
  }
  // ── The screen's blends, modern ──
  {
    const gr::FrameView fv = frame_view(hall, first, t);
    std::vector<gl::Picture> pics;
    std::vector<std::string> labels;
    fx::Screen screens[4];
    screens[1].medium = fx::Medium::water;
    screens[2].medium = fx::Medium::lava;
    screens[3].hurt = fx::take_hurt(0, 25);
    screens[3].power = true;
    const char* names4[] = {"CLEAR AIR", "UNDER WATER", "IN LAVA", "HURT, POWERED"};
    for (int k = 0; k < 4; ++k) {
      double rgba[4];
      fx::screen_blend(screens[k], rgba);
      pics.push_back(draw(d, f, first, fv, lk::Look::modern, true, 0, rgba));
      labels.push_back(names4[k]);
    }
    lab.shot(
        {"blends",
         "the screen's blends",
         "The hall in the modern look under each blend the post pass mixes in after its grade.",
         "Water a blue-green veil, lava a heavy orange, a hurt red over the power's blue.",
         {}},
        row(pics, labels), Det::measured);
  }
  // ── The sky a switch: the modern look with ce_skies on (our sky) and off (the pack's) ──
  {
    const View open = sky_view(first.scene, views[0]);
    const gr::FrameView fv = frame_view(open, first, t);
    const bool kept = f.picture.ce.skies;
    std::vector<gl::Picture> pics;
    pics.push_back(draw(d, f, first, fv, lk::Look::classic, true));
    f.picture.ce.skies = true;
    pics.push_back(draw(d, f, first, fv, lk::Look::modern, true));
    f.picture.ce.skies = false;
    pics.push_back(draw(d, f, first, fv, lk::Look::modern, true));
    f.picture.ce.skies = kept;
    const int n = differ(pics[1], pics[2]);
    lab.metric("skies.differ", double(n) / double(pics[1].width * pics[1].height), "share",
               Det::measured);
    lab.shot(
        {"skies",
         "the sky a switch",
         open.title +
             ", looking up: the classic look, then the modern with "
             "ce_skies on (our sky, its clouds and its night through the sky faces) and off (the "
             "pack's own two scrolling layers, under the modern look's lights).",
         "The modern pair differ in the sky alone; off, the sky is the classic's.",
         {}},
        row(pics, {"CLASSIC", "MODERN, CE_SKIES 1", "MODERN, CE_SKIES 0"}), Det::measured);
    if (n == 0)
      lab.fail("ce_skies 0 drew the same sky as ce_skies 1",
               "Read look.cpp's world_options (pack_sky) and bsp.wgsl's sky faces. Skill: render.");
  }
  ls::checked(lab, int(d.errors.load()), int(d.pipelines_after_load));
}

const gl::LabSpec kSpec{
    "se-world",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/app (looks, effects) over plugins/brush/app (the world's renderer, cull)",
    "shooter",
    "Does Shooter's Edge's world read in both looks from every named view, with its light styles, "
    "animated textures, effects, dynamic lights and blends, and does the cull draw the same "
    "pixels as drawing everything?",
    {{"width", "640", "a view's width, px (16:9)"},
     {"time", "20", "the views' time, s (the styles, the animation, the sky)"},
     {"pak", "",
      "a pack file holding maps/*.bsp and gfx/palette.lmp (none: the lab's own arena); its "
      "pictures are for a private page only"},
     {"maps", "start", "the maps in the pack, comma separated, without maps/ and .bsp"},
     {"cvars", "", "the picture's cvars, as a console line (\"r_fx_glow 0.5; r_lava_glow 0\")"}},
    "--width=160",
    run};
GE_LAB(kSpec);

}  // namespace
