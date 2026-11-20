// LAB `se-fly`: THE LIVE MAP's FLY (engine/overview/fly.hpp, livemap/watch.hpp, docs/goals/M62.md):
// a recording played as the window plays it, the whole map over it, then M pressed: the drawn map
// zooms in onto the followed player and, near the ground, gives way to the level drawn from above
// (its ceilings and sky faces left out), which tilts up into his eye, the demo held while it dives
// and then caught up and eased to 1x; then M again, the same way back out. Every frame is drawn by
// the window's own world frame and the live map's overlay, offscreen, at `--fps`.
//
// What it shows: six frames of the way down (the map, the zoom on the map, the blend into
// the level, the tilt, the eye, the catch-up) and one of the way out, the whole run as an animated
// GIF, the demo's rate and the eye's height as lines; `--frames=<dir>` writes every frame as a PNG
// (for a video). Measured: how far the demo's clock stands from where it would have been had
// nothing been held, once the catch-up ends
// (`behind-after`, s: about 0), and the dive's and the catch-up's lengths.
//
// With `--basedir`/`--game` and `--demo` (a path or a name) it is that recording at `--at` s on its
// timeline; without, the fixture halls and a walk round them written in code, so it runs anywhere.
//
// ge:summary Lab: the live map's fly from the map down into a player's eye and back, filmed
// ge:keywords lab se-fly fly-down dive tilt-up live-map overview catch-up demo-rate gif from-above
// ge:skill map-design labs replay
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/png.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/demo_fx.hpp"
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/livemap/watch.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace lv = shooters_edge::livemap;
namespace ov = ge::overview;
namespace ls = ge::lab::liquid_scene;

// The halls and a walk round them, in a pack.
bool fixture_pack(std::vector<uint8_t>& out, std::string& error) {
  std::vector<uint8_t> bsp;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error))
    return false;
  const cl::DemoScript s = cl::loop_script("halls", {256, 192, 24}, 120, 900);
  return ge::vfs::write_pak({{"maps/halls.bsp", bsp}, {"demo1.dem", cl::write_single_demo(s)}},
                            &out, &error);
}

// Every other pixel each way: the GIF's frame.
gl::Picture half(const ge::gpu::Image& img) {
  gl::Picture p;
  p.width = int(img.width / 2);
  p.height = int(img.height / 2);
  p.rgba.resize(size_t(p.width) * size_t(p.height) * 4);
  for (int y = 0; y < p.height; ++y)
    for (int x = 0; x < p.width; ++x)
      for (int c = 0; c < 4; ++c)
        p.rgba[(size_t(y) * size_t(p.width) + size_t(x)) * 4 + size_t(c)] =
            img.rgba[(size_t(y) * 2 * img.width + size_t(x) * 2) * 4 + size_t(c)];
  return p;
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? uint32_t(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? uint32_t(size[1]) : 540;
  const double fps = std::max(5.0, lab.arg_num("fps"));
  const double dt = 1 / fps;
  std::string error;
  cl::SearchPath files;
  const std::string basedir = lab.arg("basedir");
  if (basedir.empty()) {
    std::vector<uint8_t> pak;
    if (!fixture_pack(pak, error) || !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
  } else if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
    lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
    return;
  }
  // Quake II's recordings (.dm2, .mvd2) read and drawn by its own adapter.
  const std::string& demo_name = lab.arg("demo");
  const bool q2 = demo_name.ends_with(".dm2") || demo_name.ends_with(".mvd2");
  const cl::GameAdapter& game = q2 ? cl::q2_adapter() : cl::qw_adapter();
  cl::Timedemo bench;
  if (!cl::timedemo_start(files, demo_name, bench, error, game)) {
    lab.fail(error, "Name a demo the packs hold, or its path (--demo=demo1). Skill: replay.");
    return;
  }
  bench.paced = true;
  // On to the level and the moment asked.
  cl::TimedemoStep st = cl::timedemo_step(bench, error);
  if (st != cl::TimedemoStep::new_level && st != cl::TimedemoStep::frame) {
    lab.fail("the demo has no level: " + error, "Pass a recording of a match. Skill: replay.");
    return;
  }
  if (lab.arg_num("at") > 0) cl::demo_seek(bench, lab.arg_num("at"), error);
  bench.clock = bench.demo.state.time;
  const std::string map_name = bench.demo.state.map;
  auto map = cl::load_named_map(files, map_name, error);
  if (!map) {
    lab.fail("the demo's map " + map_name + ": " + error,
             "Pass the packs it needs. Skill: replay.");
    return;
  }
  lab.note("demo: " + bench.found + ", map " + map_name);

  auto device = ge::gpu::create_device({.label = "se-fly", .validation = 0});
  if (!device) {
    if (ls::gpu_required())
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    else
      lab.note("no GPU device: nothing drawn");
    return;
  }
  ge::gpu::Target out = ge::gpu::create_target(*device, "se-fly.final", w, h);
  shooters_edge::look::Picture picture;
  cl::WorldDrawPtr world = cl::create_world_draw(
      *device, files, *map, w, h, wgpu::TextureFormat::RGBA8Unorm, picture, {}, error,
      {bench.demo.state.models, {}, cl::demo_vweps(bench.demo.state)}, game);
  if (!world) {
    lab.fail(error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  cl::FxFrame fx;
  cl::FxState fx_state;
  cl::DemoFx demo_fx;
  fx_state.on = cl::fx_switches(picture);
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::find_file(files, "gfx/palette.lmp", store);
    fx_state.palette.assign(pal.begin(), pal.end());
  }

  // The live map, its plate made here rather than on a thread.
  lv::Watch watch;
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = lv::recording_palette(files, bench.demo.state, store);
    watch.palette.assign(pal.begin(), pal.end());
    if (watch.palette.size() < 768) {  // the fixture's pack has none: a grey ramp
      watch.palette.resize(768);
      for (size_t i = 0; i < 768; ++i) watch.palette[i] = uint8_t(i / 3);
    }
    if (!lv::live_map_level(watch.map, files, watch.palette, map->file, map_name, lv::kPlatePx,
                            &error)) {
      lab.fail("the live map of " + map_name + ": " + error, "Skill: map-design.");
      return;
    }
  }
  watch.settings.mode = 1;
  const float hud_px = float(h) * 0.12f;

  // The script: the map a moment, M (down), on until the catch-up ends and a moment more, M (up),
  // on until the map is whole again.
  const double hold_s = lab.arg_num("hold");
  enum class Phase : std::uint8_t { on_map, down, eye, up, done } phase = Phase::on_map;
  double phase_t = 0, wall = 0, press_clock = 0, press_wall = 0, behind_after = 0;
  double dive_s = 0, catch_s = 0;
  std::vector<gl::Picture> gif;
  std::vector<double> rate_s, rate_v, height_v;
  int shot = 0;
  const char* down_ids[6] = {"map", "zoom", "blend", "tilt", "eye", "catch-up"};
  const double down_at[4] = {0.0, 0.3, 0.55, 0.85};  // the way down's first four, by progress
  const std::string frames_dir = lab.arg("frames");
  if (!frames_dir.empty()) std::filesystem::create_directories(frames_dir);
  int frame_n = 0;
  bool up_shot = false;
  for (int guard = 0; guard < int(fps * 60) && phase != Phase::done; ++guard) {
    std::array<bool, 64> keys{};
    if (phase == Phase::on_map && phase_t >= hold_s) {
      keys[lv::kKeyM] = true;
      phase = Phase::down;
      phase_t = 0;
      press_clock = bench.clock;
      press_wall = wall;
    } else if (phase == Phase::eye && phase_t >= hold_s) {
      keys[lv::kKeyM] = true;
      phase = Phase::up;
      phase_t = 0;
    }
    lv::watch_keys(watch, keys, bench.demo.state, false, true);
    lv::watch_fly_step(watch, dt);
    fx_state.on = lv::watch_fx(watch, cl::fx_switches(picture));  // no flash on landing
    const double rate = lv::watch_demo_rate(watch);
    st = cl::playdemo_step(bench, dt, rate, error);
    if (st == cl::TimedemoStep::end || st == cl::TimedemoStep::error) break;
    wall += dt;
    phase_t += dt;
    if (phase == Phase::down && watch.fly.way == ov::FlyWay::none && dive_s == 0)
      dive_s = wall - press_wall;
    if (phase == Phase::down && !ov::fly_running(watch.fly)) {
      catch_s = wall - press_wall - dive_s;
      behind_after = (wall - press_wall) - (bench.clock - press_clock);
      phase = Phase::eye;
      phase_t = 0;
    }
    if (phase == Phase::up && !ov::fly_running(watch.fly) && watch.fly.p <= 0) {
      if (phase_t > 0.3) phase = Phase::done;
    }
    const cl::DemoState& ds = cl::demo_drawn(bench);
    ge::render::FrameView view = cl::demo_view(ds, ge::camera::FirstPersonRow{}.fov_y, wall);
    view.frame = uint64_t(frame_n);
    const bool above = lv::watch_fly_view(watch, view, int(w), int(h), hud_px);
    cl::from_above(*world, above);
    ge::ui::DrawList overlay;
    overlay.begin(int(w), int(h));
    lv::draw_watch(watch, ds, overlay, int(w), int(h), hud_px, ds.time);
    fx_state.models = &cl::world_models(*world);
    cl::demo_fx_step(ds, demo_fx, map.get(), view, fx_state, wall, dt, fx);
    cl::draw_world(*device, *world, view, fx, nullptr, dt, overlay, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    rate_s.push_back(wall);
    rate_v.push_back(rate);
    height_v.push_back(view.eye[1]);
    if (frame_n % 2 == 0) gif.push_back(half(img));
    if (!frames_dir.empty()) {
      char path[64];
      std::snprintf(path, sizeof path, "/%05d.png", frame_n);
      gl::write_png(frames_dir + path, gl::Picture{int(img.width), int(img.height), img.rgba});
    }
    ++frame_n;
    const gl::Picture pic{int(img.width), int(img.height), img.rgba};
    // The way down's pictures, at its progress; the catch-up's halfway through it.
    const bool catching = phase == Phase::down && watch.fly.way == ov::FlyWay::none;
    // The map before M; the zoom, the blend and the tilt by progress; the eye on landing; the
    // catch-up halfway through it.
    bool want = false;
    if (shot == 0)
      want = phase == Phase::on_map && phase_t + dt >= hold_s;
    else if (shot < 4)
      want = phase == Phase::down && !catching && watch.fly.p >= down_at[shot];
    else if (shot == 4)
      want = catching;
    else if (shot == 5)
      want = catching && watch.fly.catch_t >= watch.fly.catch_len / 2;
    if (want) {
      char tt[96];
      std::snprintf(tt, sizeof tt, "DOWN: %s (progress %.2f, demo at %.1fx)", down_ids[shot],
                    watch.fly.p, rate);
      lab.shot({std::string("down-") + down_ids[shot],
                tt,
                "The way down from the whole map into the followed player's eye: the drawn map "
                "zooming in on him, the level drawn from above taking its place near the ground "
                "with its ceilings left out, the tilt up into his eye, then the demo catching up.",
                "the drawn map growing about him with no walls; the 3D level only near the ground, "
                "lined up with the map; the view turning to his heading and tilting up as it "
                "lands; no flash on landing; no gun in hand until it lands",
                {{"map", map_name}, {"t", std::to_string(wall)}}},
               pic, gl::Det::measured);
      ++shot;
    }
    if (!up_shot && phase == Phase::up && watch.fly.p <= 0.5) {
      lab.shot({"up-half",
                "UP: halfway out",
                "The way back out: the eye climbing and tilting down, the drawn map taking over "
                "from the level and zooming out to the whole.",
                "the level from above, ceilings left out; the player below",
                {{"map", map_name}}},
               pic, gl::Det::measured);
      up_shot = true;
    }
  }
  lab.metric("dive", dive_s, "s", gl::Det::measured);
  lab.metric("catch-up", catch_s, "s", gl::Det::measured);
  lab.metric("behind-after", behind_after, "s", gl::Det::measured);
  lab.metric("frames", double(frame_n), "count", gl::Det::measured);
  if (std::fabs(behind_after) > std::max(0.25, 3 * dt))  // the demo moves by whole frames
    lab.fail("the demo stands " + std::to_string(behind_after) +
                 " s off where it would be after the catch-up",
             "Check fly_demo_rate's catch-up against the time the dive held. Skill: map-design.");
  if (!gif.empty()) lab.attach("fly.gif", gl::encode_gif_frames(gif, int(std::lround(200 / fps))));
  const auto curve = [&](const char* id, const char* title, const char* y_label,
                         const std::vector<double>& ys, const char* caption) {
    gl::LineChart c;
    c.spikes = false;
    c.frame = {title,
               map_name + ", " + std::to_string(int(fps)) + " frames a second",
               "wall clock (s)",
               y_label,
               960,
               360};
    c.traces = {{y_label, rate_s, ys}};
    lab.shot({id, title, caption, "", {{"map", map_name}}}, gl::to_picture(gl::line_chart(c).plot),
             gl::Det::measured);
  };
  curve("rate", "THE DEMO'S RATE THROUGH THE FLY", "demo rate (x)", rate_v,
        "0 while it dives (nothing missed), then fast and easing to 1x until what was held is "
        "given back; 1x all the way out.");
  curve("height", "THE EYE'S HEIGHT THROUGH THE FLY", "eye height (m)", height_v,
        "Falling fast and then slower as it lands in the eye, the same way back out.");
}

const gl::LabSpec kSpec{
    "se-fly",
    gl::LabKind::drive,
    "game:shooters_edge",
    "the live map's fly from the whole map down into the followed player's eye and back",
    "map-design",
    "Does M dive from the map into the player's eye smoothly (the level from above in the map's "
    "place, the tilt as it lands) and does the demo catch up with what the dive held?",
    {
        {"size", "960,540", "the frame's width and height, px"},
        {"fps", "30", "frames a second of the fly drawn"},
        {"hold", "0.6", "s on the map before M, and in the eye before M again"},
        {"basedir", "", "a folder holding a game folder: play its demo instead of the fixture"},
        {"game", ge::classicenhance::kProfiles[0].base_dir.data(),
         "the game folder under --basedir"},
        {"demo", "demo1", "the demo to play, by name or path"},
        {"at", "0", "s on the demo's timeline to start from"},
        {"frames", "", "a folder: every frame written there as a PNG"},
    },
    "--size=320,180 --fps=10",
    run,
};
GE_LAB(kSpec);

}  // namespace
