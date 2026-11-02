// LAB `gpu-bsp-cutout`: SEE-THROUGH TEXELS CUT IN A CLASSIC MAP (plugins/brush/app/draw_data.hpp's
// `Scene::cover`, bsp.wgsl's cut): the fixture halls drawn by the v2 renderer as the map paints
// them, beside the same view with two textures handed in full colour with alpha, the way a game's
// PNG or TGA stands in for a map's texture (`MapPicture::rgba`): the hall's brick as a lattice
// whose gaps are see-through, and its floor as a fence texture (a `{` name, index 255 a hole).
// Through the gaps the void past the map shows; the solid bars and slabs keep the light they had.
//
// ge:summary Lab: a map's see-through texels (a picture's alpha, a fence texture's holes) cut
// ge:keywords lab bsp cutout cover alpha fence see-through lattice png tga replacement-texture
// ge:keywords gpu-bsp-cutout before-after
// ge:skill brush render labs
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_sky.hpp"
#include "brush/app/draw_data.hpp"
#include "brush/pictures.hpp"
#include "brush/write.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "render/bloom.hpp"
#include "render/clouds.hpp"
#include "render/frame.hpp"
#include "render/post.hpp"
#include "render/sky.hpp"

namespace {

namespace gl = ge::lab;
namespace gr = ge::render;
namespace ba = ge::brush::app;
namespace ls = ge::lab::liquid_scene;
using gl::Det;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

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
  gr::Bloom bloom;
  gr::Post post;
  gr::FrameRecorder rec;
  ge::gpu::Target out;
  uint32_t w = 0, h = 0;
};

struct MapState {
  ba::Scene scene;
  ba::BspDraw bsp;
  ba::SkyPick sky;
};

gl::Picture draw(ge::gpu::Device& d, Frame& f, MapState& m, gr::FrameView v, int frames) {
  ba::Options o;
  o.sky_light = m.sky.sky_light;
  o.sky_seen = m.sky.sky_seen;
  for (int j = 0; j < 3; ++j) o.sky_tint[j] = m.sky.tint[j];
  gl::Picture pic;
  for (int k = 0; k < frames; ++k) {
    f.rec.begin_frame(d);
    gr::fill_uniforms(f.staged, v, f.w, f.h, &f.history);
    d.queue.WriteBuffer(f.uniforms, 0, &f.staged, sizeof f.staged);
    gr::prepare_sky(f.sky, v);
    ba::prepare_bsp(d, m.bsp, v, o);
    wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
    gr::encode_sky_tables(f.sky, f.pipelines, f.rec, e);
    ba::encode_bsp(m.bsp, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_sky(f.sky, f.pipelines, f.rec, e, f.targets, f.frame);
    gr::encode_clouds(f.clouds, f.pipelines, f.rec, e, f.targets, f.frame, v, f.staged);
    ba::encode_bsp_veil(m.bsp, f.pipelines, f.rec, e, f.targets);
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

// The brick as a lattice: warm bars two texels in eight, the gaps between them see-through.
void lattice(ge::brush::MapPicture& p) {
  const uint32_t w = std::max(p.width, 16u) * 2, h = std::max(p.height, 16u) * 2;
  p.rgba_width = w;
  p.rgba_height = h;
  p.rgba.assign(size_t(w) * h * 4, 0);
  for (uint32_t y = 0; y < h; ++y)
    for (uint32_t x = 0; x < w; ++x) {
      uint8_t* t = &p.rgba[(size_t(y) * w + x) * 4];
      const bool bar = (x % 16) < 4 || (y % 16) < 4;
      t[0] = 190, t[1] = 120, t[2] = 80, t[3] = bar ? 255 : 0;
    }
}

void run(gl::Lab& lab) {
  const auto w = static_cast<uint32_t>(lab.arg_num("width"));
  const uint32_t h = w * 9 / 16;
  const int frames = std::max(1, static_cast<int>(lab.arg_num("frames")));
  ge::brush::Fixture fx = ge::brush::sample_fixture();
  fx.rooms[0].floor = "{grate";  // the hall's floor a fence texture
  std::vector<uint8_t> bytes;
  std::string err;
  if (!ge::brush::write_bsp(fx, ge::brush::Family::v29, &bytes, &err)) {
    lab.fail("the fixture did not compile: " + err,
             "next: read plugins/brush/fixture.cpp's sample_fixture. Skill: brush.");
    return;
  }
  ba::Pack pack;
  pack.bytes = bytes;
  pack.files.push_back({"maps/halls.bsp", 0, uint32_t(bytes.size())});
  std::vector<uint8_t> palette(768);
  for (int i = 0; i < 256; ++i)
    for (int k = 0; k < 3; ++k)
      palette[size_t(i) * 3 + size_t(k)] = uint8_t(std::min(250, 90 + i * 3 / 4));

  // The pictures as a game hands them: the map's own, the brick a lattice with alpha, and the
  // fence floor's every fourth row index 255 (the fixture writes no hole of its own).
  std::vector<ge::brush::MapPicture> pics = ge::brush::read_map_pictures(bytes);
  std::vector<uint8_t> grate;
  int swapped = 0;
  for (ge::brush::MapPicture& p : pics) {
    if (p.name == "brick") lattice(p), swapped++;
    if (p.name == "{grate" && p.level[0] != nullptr) {
      grate.assign(p.level[0], p.level[0] + size_t(p.width) * p.height);
      for (uint32_t y = 0; y < p.height; ++y)
        if (y % 8 < 3)
          for (uint32_t x = 0; x < p.width; ++x) grate[size_t(y) * p.width + x] = 255;
      p.level[0] = grate.data();
      swapped++;
    }
  }
  lab.metric("textures.handed", swapped, "count");
  if (swapped != 2)
    lab.fail("the fixture's brick and {grate textures were not both found (" +
                 std::to_string(swapped) + ")",
             "next: read the fixture's texture names in plugins/brush/fixture.cpp. Skill: brush.");
  MapState own, cut;
  ba::TextureSource source;
  source.pictures = &pics;
  if (!ba::load_scene(pack, "maps/halls.bsp", palette, &own.scene, &err) ||
      !ba::load_scene(pack, "maps/halls.bsp", palette, &cut.scene, &err, source)) {
    lab.fail("the halls did not load: " + err, "next: read the reason above. Skill: brush.");
    return;
  }
  size_t holes = 0;
  if (!cut.scene.cover.levels.empty())
    for (uint8_t c : cut.scene.cover.levels[0]) holes += c < 128;
  lab.metric("cover.see-through-texels", double(holes), "count");
  lab.metric("cover.own-map", own.scene.cover.levels.empty() ? 0 : 1, "count");
  if (holes == 0 || !own.scene.cover.levels.empty())
    lab.fail("the cover is " + std::string(holes == 0 ? "empty with the pictures handed"
                                                      : "set for the map's own pictures"),
             "next: read draw_data.cpp's cover. Skill: brush.");
  own.sky = ba::pick_sky(own.scene.sky_colours);
  cut.sky = own.sky;

  auto device = ge::gpu::create_device({.label = "gpu-bsp-cutout", .validation = 1});
  if (!device) {
    if (ls::gpu_required()) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: no pictures.");
    ls::placeholders(lab, {"cutout-hall", "cutout-floor"});
    ls::checked(lab, 0, 0);
    return;
  }
  ge::gpu::Device& d = *device;
  Frame f;
  f.w = w;
  f.h = h;
  f.targets = gr::create_targets(d, "gpu-bsp-cutout", w, h);
  f.uniforms = gr::make_buffer(d, "gpu-bsp-cutout.uniforms", sizeof(gr::FrameUniforms),
                               wgpu::BufferUsage::Uniform);
  f.layout = gr::frame_layout(d);
  gr::create_sky(d, f.pipelines, f.sky, f.uniforms, f.layout);
  gr::create_clouds(d, f.pipelines, f.clouds, f.layout);
  gr::BloomSettings bs;
  bs.strength = 0.05;
  gr::create_bloom_pipelines(d, f.pipelines, f.bloom, bs);
  gr::PostSettings ps;
  ps.bloom = 0.05;
  gr::create_post(d, f.pipelines, f.post, f.layout, wgpu::TextureFormat::RGBA8Unorm, ps);
  for (MapState* m : {&own, &cut}) ba::create_bsp(d, f.pipelines, m->bsp, m->scene, f.layout);
  if (!f.pipelines.finish(d)) {
    lab.fail("the BSP renderer's pipelines did not build",
             "Read the gpu: lines above (plugins/brush/app/bsp.wgsl). Skill: render.");
    return;
  }
  ge::gpu::end_load(d);
  f.frame = gr::frame_group(d, f.layout, f.uniforms, f.sky, nullptr, nullptr);
  gr::bind_cloud_targets(d, f.clouds, f.targets);
  const wgpu::TextureView inputs[2] = {f.targets.hdr_view, f.targets.hdr_view};
  gr::create_bloom_targets(d, f.bloom, w, h, inputs);
  f.post_group = gr::post_group(d, f.post, f.targets.hdr_view, f.bloom.view);
  f.rec.init(d, "gpu-bsp-cutout.timer", 32);
  f.out = ge::gpu::create_target(d, "gpu-bsp-cutout.final", w, h);
  for (MapState* m : {&own, &cut}) ba::create_bsp_targets(d, m->bsp, f.targets);

  struct View {
    const char *id, *title, *caption, *look;
    double eye[3], heading, pitch;
  };
  const View views[] = {
      {"cutout-hall",
       "The hall's brick as a lattice",
       "From the hall's west end toward its east wall and door. Left: the map's own brick. Right: "
       "the brick handed as a picture with alpha, its gaps see-through.",
       "Right: the walls a grid of bars with the sky past the map in every gap, the floor striped "
       "with the fence's holes; the door and ceiling as on the left.",
       {2.5, 1.6, 4.8},
       90 * kDeg,
       -4 * kDeg},
      {"cutout-floor",
       "The hall's floor as a fence texture",
       "Looking down at the hall's floor. Left: solid. Right: the floor a fence texture, its rows "
       "of index 255 cut through.",
       "Right: stripes of void across the floor where its holes are, the slabs between them lit.",
       {4.0, 3.2, 4.8},
       90 * kDeg,
       -60 * kDeg},
  };
  for (const View& v : views) {
    gr::FrameView fv;
    for (int k = 0; k < 3; ++k) fv.eye[k] = v.eye[k];
    fv.heading = v.heading;
    fv.pitch = v.pitch;
    fv.style = own.sky.style;
    fv.sun_elevation = 30 * kDeg;
    fv.altitude_m = 100;
    fv.time_s = 40;
    fv.fov_y = 1.25;
    fv.near_m = 0.05;
    const gl::Picture before = draw(d, f, own, fv, frames);
    const gl::Picture after = draw(d, f, cut, fv, frames);
    gl::Picture sheet = gl::create_picture(int(w) * 2 + 30, int(h) + 40, {250, 250, 248});
    gl::draw_text(sheet, 10, 6, "THE MAP'S OWN", {24, 26, 32});
    gl::draw_text(sheet, 20 + int(w), 6, "WITH ALPHA CUT", {24, 26, 32});
    ls::paste(sheet, before, 10, 20);
    ls::paste(sheet, after, 20 + int(w), 20);
    lab.shot(
        {v.id,
         v.title,
         v.caption,
         v.look,
         {{"heading", ls::fixed(v.heading / kDeg, 0)}, {"pitch", ls::fixed(v.pitch / kDeg, 0)}}},
        sheet, Det::measured);
  }
  ls::checked(lab, int(d.errors.load()), int(d.pipelines_after_load));
}

const gl::LabSpec kSpec{
    "gpu-bsp-cutout",
    gl::LabKind::picture,
    "plugin:brush",
    "plugins/brush/app/draw_data.hpp",
    "brush",
    "Are a map's see-through texels (a full-colour picture's alpha, a fence texture's holes) cut "
    "out of its faces, with the solid texels drawn as before?",
    {{"width", "480", "a pane's width, px (16:9)"},
     {"frames", "4", "the frames drawn before each picture"}},
    "--width=128 --frames=2",
    run};
GE_LAB(kSpec);

}  // namespace
