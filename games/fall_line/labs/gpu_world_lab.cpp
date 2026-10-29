// LAB `world`: the game's own frame, photographed. It generates a seed's map, makes Fall Line's
// frame (games/fall_line/app/frame.hpp: the pass table over engine/render) on a headless device
// (the software adapter in the GPU-less container and on CI) and renders each named view
// (views.hpp) offscreen, reads it back and hands it to `Lab::shot` with its caption. Then the
// bot skis the map (with --rivals beside him) and each --moments second of the run is shot from
// the chase camera, the racers drawn as figures. --at shoots each named view at the sky's moments,
// so a sequence shows the cloud on the wind. Beside each picture: the tiles the terrain's GPU
// cull drew, what the instances' cull did (drawn early, drawn late, hidden), every pass's GPU ms
// (measured: a software adapter's times say nothing about a real GPU) and the frame's CPU half.
//
// The frame renders at --scale of the picture's size and TAA-U brings it up (--taa=0: no TAA, at
// full size), each view a camera cut drawn --settle frames before the capture. Then, per view:
// the SHIMMER (the mean change of a channel, in 8-bit levels, from one more frame of the still
// camera: what the jitter leaves after the resolve) and the OVERDRAW probe (the fragments the
// terrain shades per covered pixel with the depth prepass, which must be 1 but for coincident
// surfaces, and without it, the cost the prepass saves), both over the ground and the instances;
// and the CULL check: the same still drawn again with the depth pyramid's culling off must be the
// same picture, or the cull dropped something that shows (a difference is drawn again first: on a
// hardware adapter one that does not come back is a warning, `cull-unrepeated`, never a fault). And
// the sun's CASCADES over the whole run, without a device: how many frames of the chase camera
// redraw the near and the far one.
//
// What must hold on every row is deterministic (the views shot, the pass table, the render size
// and jitter cycle, no pipeline after load, no GPU error); pixels and timings are measured. Without
// an adapter it notes that and draws placeholders, so the digest is the same on every row;
// GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// --console=1 draws the last scene once more with the drop-down console down over it, as the
// window draws it (frame.hpp's `ui` row: ui/console.hpp's list through the 2D pass after post): a
// few lines in its scrollback and a command half typed, its suggestions under the panel.
//
// --cfg runs console commands over Fall Line's cvars first (cfg/cvars.hpp), so a sky a server
// picks (sv_weather, sv_snowfall, sv_fog, sv_wind, sv_winddir, sv_cover, sv_thick,
// sv_cloudheight, sv_cloudspeed, sv_hour, sv_skyrate) and r_clouds are photographed as the game
// draws them (sim/sky.hpp).
//
// ge:summary Lab: Fall Line's frame from named views of a seed, captured headless
// ge:keywords lab world picture screenshot views frame terrain sky post gpu-capture headless
// ge:keywords tiles-drawn gpu-ms cpu-ms pass-table fall-line taa render-scale shimmer overdraw
// ge:keywords instances forest figures run-moments chase-camera occlusion-culling cull-check
// ge:keywords sun-shadow cascade-redraws shadow-cache drop-down-console overlay ui-pass
// ge:skill render labs
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "console/console.hpp"
#include "console/view.hpp"
#include "core/output.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/sky_type.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/cfg/cvars.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/simulate.hpp"
#include "fall_line/sim/sky.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "render/shadows.hpp"
#include "render/taa.hpp"
#include "ui/console.hpp"
#include "ui/draw.hpp"

namespace {

using ge::lab::Det;

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

// The views a run shoots: the --views list, or every named view.
std::vector<std::string> views_of(const ge::lab::Lab& lab) {
  std::vector<std::string> out;
  const std::string& list = lab.arg("views");
  if (list.empty() || list == "all") {
    for (std::string_view n : fall_line::app::view_names()) out.emplace_back(n);
    return out;
  }
  size_t i = 0;
  while (i <= list.size()) {
    const size_t j = list.find(',', i);
    const size_t end = j == std::string::npos ? list.size() : j;
    if (end > i) out.push_back(list.substr(i, end - i));
    i = end + 1;
  }
  return out;
}

// A comma-separated list of seconds: --moments (the run's, each a shot from the chase camera) or
// --at (the sky's, each named view shot at each).
std::vector<double> seconds_of(const ge::lab::Lab& lab, const char* arg) {
  std::vector<double> out;
  const std::string& list = lab.arg(arg);
  size_t i = 0;
  while (i < list.size()) {
    const size_t j = list.find(',', i);
    const size_t end = j == std::string::npos ? list.size() : j;
    if (end > i) out.push_back(std::strtod(list.substr(i, end - i).c_str(), nullptr));
    i = end + 1;
  }
  return out;
}

// The terrain's indirect instance count, read back now (tiles drawn the last frame).
uint32_t tiles_drawn(ge::gpu::Device& device, const wgpu::Buffer& indirect) {
  wgpu::BufferDescriptor desc{};
  desc.label = "world.tiles.readback";
  desc.size = 20;
  desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer rb = device.device.CreateBuffer(&desc);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(indirect, 0, rb, 0, 20);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  uint32_t count = 0;
  ge::gpu::wait(device, rb.MapAsync(wgpu::MapMode::Read, 0, 20, wgpu::CallbackMode::WaitAnyOnly,
                                    [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  if (const void* p = rb.GetConstMappedRange(0, 20)) {
    std::memcpy(&count, static_cast<const uint8_t*>(p) + 4, 4);
  }
  rb.Unmap();
  return count;
}

// The overdraw of the ground and the instances over the frame's last depth: the mean count over
// covered pixels after the prepass (`equal`), then without it (`greater`, which rewrites the
// depth; the next view's frames clear it). Blocks.
void overdraw(ge::gpu::Device& device, fall_line::app::GameFrame& f, const wgpu::Texture& count,
              const wgpu::TextureView& count_view, double* with, double* without) {
  const auto& t = f.targets;
  const std::vector<float> depth = ge::render::read_depth(device, t);
  for (int pass = 0; pass < 2; ++pass) {
    wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
    ge::render::encode_terrain_overdraw(f.terrain, f.pipelines, encoder, t, f.frame_group,
                                        count_view, pass == 0);
    ge::render::encode_instances_overdraw(f.families, f.instancing, f.pipelines, encoder, t,
                                          f.frame_group, f.instance_light, count_view, pass == 0);
    wgpu::CommandBuffer commands = encoder.Finish();
    device.queue.Submit(1, &commands);
    const std::vector<float> n =
        ge::render::read_red(device, count, 0, t.width, t.height, wgpu::TextureFormat::R16Float);
    double sum = 0, covered = 0;
    for (size_t i = 0; i < n.size(); ++i) {
      if (depth[i] > 0) {
        sum += n[i];
        ++covered;
      }
    }
    (pass == 0 ? *with : *without) = covered > 0 ? sum / covered : 0;
  }
}

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// The pixels two stills differ in, and the box round them.
struct CullDiff {
  double pixels = 0;
  int x0 = 0, y0 = 0, x1 = -1, y1 = -1;
  std::string describe() const {
    if (pixels == 0) return "0 pixels";
    return fixed(pixels, 0) + (pixels == 1 ? " pixel" : " pixels") + " in x " + std::to_string(x0) +
           ".." + std::to_string(x1) + ", y " + std::to_string(y0) + ".." + std::to_string(y1);
  }
};

CullDiff cull_diff(const ge::gpu::Image& a, const ge::gpu::Image& b, int width) {
  CullDiff d;
  d.x0 = width;
  d.y0 = 1 << 30;
  for (size_t i = 0; i + 3 < std::min(a.rgba.size(), b.rgba.size()); i += 4) {
    if (std::memcmp(&a.rgba[i], &b.rgba[i], 3) == 0) continue;
    const int x = static_cast<int>(i / 4 % static_cast<size_t>(width));
    const int y = static_cast<int>(i / 4 / static_cast<size_t>(width));
    ++d.pixels;
    d.x0 = std::min(d.x0, x);
    d.x1 = std::max(d.x1, x);
    d.y0 = std::min(d.y0, y);
    d.y1 = std::max(d.y1, y);
  }
  return d;
}

// The culled still darkened, with every pixel the unculled one differs in painted red.
ge::lab::Picture diff_picture(const ge::gpu::Image& culled, const ge::gpu::Image& all, int width,
                              int height) {
  ge::lab::Picture p{width, height, culled.rgba};
  for (size_t i = 0; i + 3 < std::min(p.rgba.size(), all.rgba.size()); i += 4) {
    const bool differs = std::memcmp(&culled.rgba[i], &all.rgba[i], 3) != 0;
    for (int c = 0; c < 3; ++c) {
      p.rgba[i + c] = static_cast<uint8_t>(differs ? (c == 0 ? 255 : 0) : p.rgba[i + c] / 3);
    }
  }
  return p;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  // --cfg: console commands over Fall Line's cvars, as a cfg would set them (the sky's sv_ cvars
  // and r_clouds reach the map and the picture here; nothing else is read from them).
  ge::console::Console console;
  fall_line::cfg::Settings cvars;
  fall_line::cfg::add_cvars(console, cvars);
  console.execute(lab.arg("cfg"));
  console.cvars().apply_latched();
  for (const ge::console::Problem& p : console.problems()) {
    lab.fail("--cfg: " + p.what, p.next + " Skill: console.");
  }
  const double hour = lab.arg_num("hour") >= 0 ? lab.arg_num("hour") : cvars.hour;
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int settle = static_cast<int>(lab.arg_num("settle"));
  fall_line::app::FrameOptions options;
  options.taa = lab.arg_num("taa") != 0;
  options.scale = lab.arg_num("scale");
  options.overdraw_probe = true;
  options.snow = lab.arg_num("snow") != 0;
  options.trails = lab.arg_num("trails") != 0;
  options.reach_m = lab.arg_num("reach");
  options.clouds = cvars.clouds;
  options.cover = cvars.cover;
  options.thick = cvars.thick;
  options.cloud_height = cvars.cloud_height;
  options.cloud_speed = cvars.cloud_speed;
  options.sun_size = cvars.disc_sun;
  options.moon_size = cvars.disc_moon;
  if (const std::string problem = fall_line::app::cloud_stack_of(cvars, options);
      !problem.empty()) {
    lab.fail("--cfg: " + problem, "Skill: console.");
  }
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || settle < 1 ||
      options.scale < 0.25 || options.scale > 1 || options.reach_m < 0) {
    lab.fail(
        "--width and --height must be 1..4096, --settle at least 1, --scale 0.25..1 and "
        "--reach at least 0",
        "next: pass a size such as --width=640 --height=360 --scale=1 --reach=0.");
    return;
  }
  const double scale = options.taa ? options.scale : 1.0;
  const uint32_t render_w = ge::render::taa_render_size(static_cast<uint32_t>(width), scale);
  const uint32_t render_h = ge::render::taa_render_size(static_cast<uint32_t>(height), scale);
  lab.metric("render-width", render_w, "px");
  lab.metric("render-height", render_h, "px");
  lab.metric("jitter-phases", options.taa ? ge::render::taa_phases(scale) : 0, "count");
  const std::vector<std::string> views = views_of(lab);
  const std::string faults = fall_line::app::pass_table_faults();
  lab.metric("pass-table-faults", faults.empty() ? 0 : 1, "count");
  if (!faults.empty()) {
    lab.fail("Fall Line's pass table is out of order: " + faults,
             "Fix kPassTable in games/fall_line/app/frame.cpp. Skill: render.");
  }
  lab.metric("views", static_cast<double>(views.size()), "count");

  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  ge::mountain::mapgen::Level built = ge::mountain::mapgen::generate_level(seed, opts);
  if (lab.arg_num("day") > 0) built.sun.day_of_year = lab.arg_num("day");
  if (const std::string problem = fall_line::sim::server_sky(cvars, built); !problem.empty()) {
    lab.fail("--cfg: " + problem, "Skill: console.");
  }
  // The kind of sky (sv_skytype and its knobs, app/sky_type.hpp).
  ge::render::SkyStyle style;
  if (const std::string problem = fall_line::app::sky_style_of(cvars, style); !problem.empty()) {
    lab.fail("--cfg: " + problem, "Skill: console.");
  }
  ge::render::MoonSet moons;
  if (const std::string problem = fall_line::app::moon_set_of(cvars, moons); !problem.empty()) {
    lab.fail("--cfg: " + problem, "Skill: console.");
  }
  const auto shared = std::make_shared<const ge::mountain::mapgen::Level>(std::move(built));
  const ge::mountain::mapgen::Level& level = *shared;
  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const std::string hour_s = hour < 0 ? fixed(level.sun.hour, 2) + " (the map's)" : fixed(hour, 2);

  // What is shot: the named views over the empty map, then the run's moments.
  struct Scene {
    std::string name;
    std::string caption;
    ge::render::FrameView view;
    std::vector<ge::skier::SkierState> skiers;
    // The racers every fourth step since the last moment: the tracks laid before it is shot.
    std::vector<std::vector<ge::skier::SkierState>> laid;
  };
  std::vector<Scene> scenes;
  // Each named view at each of the sky's --at seconds (more than one: a sequence, each shot
  // named `<view>+<s>`), the cloud carried on by the wind between them.
  std::vector<double> ats = seconds_of(lab, "at");
  if (ats.empty()) ats.push_back(0);
  for (const std::string& name : views) {
    for (double at : ats) {
      Scene sc{name, std::string(fall_line::app::view_caption(name)), {}, {}, {}};
      if (ats.size() > 1) {
        sc.name += "+" + fixed(at, 1);
        sc.caption += " At " + fixed(at, 1) + " s.";
      }
      sc.view.style = style;
      sc.view.moons = moons;
      sc.view.moon_size = cvars.disc_moon;  // a lens on a moon frames it as it is drawn
      fall_line::app::day_view(level, hour, sc.view);
      sc.view.time_s = at;
      if (!fall_line::app::make_view(level, name, sc.view)) {
        lab.fail("no view named '" + name + "'",
                 "next: pick from games/fall_line/app/views.hpp (or --views=all).");
        break;
      }
      scenes.push_back(std::move(sc));
    }
  }
  const std::vector<double> moments = seconds_of(lab, "moments");
  const int rivals = static_cast<int>(lab.arg_num("rivals"));
  if (!moments.empty()) {
    fall_line::sim::Driver run(shared, std::max(rivals, 0));
    int steps = 0;
    for (double t : moments) {
      Scene sc;
      while (run.state().t < t && !run.over()) {
        run.step(fall_line::sim::bot_controls(run.state(), 0, run.plan()));
        if (++steps % 4 == 0) {
          sc.laid.emplace_back();
          for (const auto& racer : run.state().racers) sc.laid.back().push_back(racer.skier);
        }
      }
      sc.name = "run-" + fixed(t, 0);
      sc.caption = "The bot's run " + fixed(run.state().t, 1) +
                   " s after the start, from the chase camera behind and over him; the racers "
                   "are the skier model, posed from each one's state and skinned on the GPU, "
                   "each in his own jacket.";
      for (const auto& racer : run.state().racers) sc.skiers.push_back(racer.skier);
      fall_line::app::day_view(level, hour, sc.view);
      sc.view.style = style;
      // The sky's clock moved on by sv_skyrate (cfg) over the run so far.
      if (cvars.sky_rate > 0) {
        fall_line::cfg::Settings at = cvars;
        at.hour = hour;
        fall_line::app::sky_view(level, fall_line::sim::sky_hour(at, level, run.state().t),
                                 sc.view);
      }
      fall_line::app::chase_view(level, sc.skiers[0], sc.view);
      scenes.push_back(std::move(sc));
    }
  }
  lab.metric("moments", static_cast<double>(moments.size()), "count");

  // The sun's cascades over the whole run: the chase camera at 60 frames a second, each frame
  // deciding (render/shadows.hpp `update_cascade`, the decision the frame makes) whether a cascade
  // is drawn again. Deterministic and with no device: the counts are what the frame would draw.
  {
    fall_line::sim::Driver run(shared, 0);
    const ge::render::ShadowSettings set;
    std::array<ge::render::ShadowCascade, ge::render::kCascades> cascades{};
    std::array<int, ge::render::kCascades> redraws{};
    int frames = 0;
    ge::render::FrameView v;
    fall_line::app::day_view(level, hour, v);
    double sun[3];
    ge::render::sun_direction(v, sun);
    while (!run.over()) {
      fall_line::app::chase_view(level, run.state().racers[0].skier, v);
      for (int c = 0; c < ge::render::kCascades; ++c) {
        float m[16];
        if (ge::render::update_cascade(set, c, sun, v.eye, cascades[static_cast<size_t>(c)], m)) {
          ++redraws[static_cast<size_t>(c)];
        }
      }
      ++frames;
      for (int k = 0; k < 2 && !run.over(); ++k) {
        run.step(fall_line::sim::bot_controls(run.state(), 0, run.plan()));
      }
    }
    lab.metric("cascade-frames", frames, "count");
    lab.metric("cascade-redraws.near", redraws[0], "count");
    lab.metric("cascade-redraws.far", redraws[1], "count");
    lab.metric("cascade-redraws-per-frame",
               frames > 0 ? static_cast<double>(redraws[0] + redraws[1]) / frames : 0, "count");
  }

  auto shot_of = [&](const Scene& sc) {
    std::string title = "SEED " + seed_s + ", " + sc.name;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return ge::lab::Shot{
        sc.name,
        title,
        sc.caption +
            " The ground is snow (white), the packed piste (a cooler grey-blue) and rock where it "
            "is too steep for snow, lit by the sun and the sky, hazed with distance; the sky is "
            "the atmosphere's at the map's hour.",
        "ground meets the sky at a clean horizon; no cracks, holes or seams between the "
        "terrain's rings (the ring edges sit at 64, 128, 256 m ... from the eye); trees stand on "
        "the ground, not over it, and none pops or is missing at the screen's edge; a figure "
        "stands on its skis on the snow; far ridges bluer and paler than near ones; the sun's "
        "side brighter than the shadow side; no NaN (black or magenta) pixels",
        {{"seed", seed_s},
         {"view", sc.name},
         {"hour", hour_s},
         {"size", std::to_string(width) + "x" + std::to_string(height)},
         {"render", std::to_string(render_w) + "x" + std::to_string(render_h) +
                        (options.taa ? ", TAA-U" : ", no TAA")},
         {"reach", options.reach_m > 0 ? fixed(options.reach_m, 0) + " m, misted" : "all"},
         {"cfg", lab.arg("cfg").empty() ? "none" : lab.arg("cfg")}}};
  };

  // The console's still: the last scene with the drop-down console down over it.
  const bool with_console = lab.arg_num("console") != 0 && !scenes.empty();
  auto console_shot = [&] {
    ge::lab::Shot shot = shot_of(scenes.back());
    shot.id = "console";
    shot.title = "SEED " + seed_s + ", CONSOLE";
    shot.caption =
        "The last scene again with the drop-down console down over the top half, drawn by the "
        "2D pass after post as the window draws it: the con_bg gradient over the game, the "
        "scrollback coloured by kind, the input line with a cvar's name half typed and its hint, "
        "the suggestions under the panel's edge.";
    shot.look_for =
        "the panel covers the top of the frame and the mountain shows under it and through it; "
        "the text is crisp and inside the panel; the suggestion list hangs under the yellow "
        "edge; the mountain below the panel is the same picture as the scene";
    return shot;
  };

  auto device = ge::gpu::create_device({.label = "world", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    for (const Scene& sc : scenes) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(shot_of(sc), none, Det::measured);
    }
    if (with_console) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      lab.shot(console_shot(), none, Det::measured);
    }
    // The digest hashes in call order: the shots, then these, as the device path does.
    lab.metric("overdraw-faults", 0, "count");
    lab.metric("cull-faults", 0, "count");
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  const auto& r = device->report;
  lab.note("adapter: " + r.name + " / " + r.driver + " / " + r.backend +
           (r.software ? " (software)" : ""));

  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(width),
                                            static_cast<uint32_t>(height),
                                            wgpu::TextureFormat::RGBA8Unorm, options);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "world.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  const wgpu::Texture count = ge::render::make_texture(
      *device, "world.overdraw", render_w, render_h, 1, wgpu::TextureFormat::R16Float,
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);
  const wgpu::TextureView count_view = count.CreateView();
  int overdraw_faults = 0, cull_faults = 0, cull_unrepeated = 0, cull_hardware = 0;

  // A still of the scene: a cut, then `settle` frames.
  auto still = [&](Scene& sc) {
    fall_line::app::cut(*frame);
    for (int i = 0; i < settle; ++i) {
      sc.view.frame = static_cast<uint64_t>(i);
      fall_line::app::draw(*device, *frame, sc.view, {sc.skiers}, target.view);
    }
    return ge::gpu::read_now(*device, target);
  };

  // Tiles and instances depend on the adapter's float rounding at the frustum's edge, so the
  // per-view costs are a note, not a table (a table's cells are hashed).
  std::string costs =
      "per view (tiles drawn / trees drawn early+late, hidden / draws / dispatches):";
  for (Scene& sc : scenes) {
    const std::string& name = sc.name;
    for (const auto& racers : sc.laid) fall_line::app::track(*frame, {racers});
    const ge::gpu::Image img = still(sc);
    frame->recorder.wait(*device);
    // How far away each pixel is, off this still's depth, for a diff (`--compare`).
    ge::lab::DistanceMap distance{static_cast<int>(frame->targets.width),
                                  static_cast<int>(frame->targets.height),
                                  ge::render::read_distance(*device, frame->targets, sc.view)};
    const ge::render::InstanceStats trees = ge::render::read_instance_stats(*device, frame->forest);
    const ge::render::InstanceStats figures =
        ge::render::read_instance_stats(*device, frame->figures);
    // One more frame of the still camera: what changes is the jitter's shimmer.
    {
      sc.view.frame = static_cast<uint64_t>(settle);
      fall_line::app::draw(*device, *frame, sc.view, {sc.skiers}, target.view);
      const ge::gpu::Image next = ge::gpu::read_now(*device, target);
      double diff = 0;
      for (size_t i = 0; i < std::min(img.rgba.size(), next.rgba.size()); ++i) {
        if (i % 4 != 3) diff += std::abs(int{img.rgba[i]} - int{next.rgba[i]});
      }
      lab.metric(name + ".shimmer", diff / (static_cast<double>(width) * height * 3), "levels",
                 Det::measured);
    }
    double with = 0, without = 0;
    overdraw(*device, *frame, count, count_view, &with, &without);
    lab.metric(name + ".shaded-per-pixel", with, "count", Det::measured);
    lab.metric(name + ".overdraw-without-prepass", without, "count", Det::measured);
    // Over 1 only where two surfaces lie at the very same depth (two clipmap levels meeting at a
    // ring's edge), which both pass `equal`: a few in ten thousand. Under 1 is a pixel the colour
    // pass missed.
    if (with != 0 && (with < 1.0 || with > 1.001)) ++overdraw_faults;
    const auto& stats = frame->recorder.stats;
    const uint32_t tiles = tiles_drawn(*device, frame->terrain.indirect);
    lab.metric(name + ".tiles", tiles, "count", Det::measured);
    lab.metric(name + ".trees-early", trees.early, "count", Det::measured);
    lab.metric(name + ".trees-late", trees.late, "count", Det::measured);
    lab.metric(name + ".trees-hidden", trees.hidden, "count", Det::measured);
    lab.metric(name + ".trees-outside", trees.frustum + trees.small, "count", Det::measured);
    lab.metric(name + ".figures", figures.early + figures.late, "count", Det::measured);
    lab.metric(name + ".cpu-ms", stats.cpu_ms, "ms", Det::measured);
    for (int p = 0; p < stats.count; ++p) {
      const auto& ps = stats.passes[static_cast<size_t>(p)];
      if (!std::isnan(ps.gpu_ms)) {
        lab.metric(name + ".gpu-ms." + std::string(ps.name), ps.gpu_ms, "ms", Det::measured);
      }
    }
    costs += " " + name + " " + std::to_string(tiles) + " / " + std::to_string(trees.early) + "+" +
             std::to_string(trees.late) + ", " + std::to_string(trees.hidden) + " / " +
             std::to_string(stats.draws()) + " / " + std::to_string(stats.dispatches()) + ";";
    // THE CULL CHECK: the same still with the pyramid's culling off draws every instance in the
    // frustum; on the software adapter the two pictures are the same bits, so a pixel that differs
    // is one the cull dropped although it shows.
    // A cull that drops what shows drops it every time it draws the same frames, so a difference
    // is drawn again before it is called the cull's. On the software adapter any difference is a
    // fault. On a hardware adapter it is measured and warned, with the pictures, and never gates:
    // Apple's paravirtual Metal drops a pixel of a tree on some runs and not others in one process
    // (docs/goals/M7d1.md), so a count there would move the digest from run to run.
    {
      frame->options.occlusion = false;
      const ge::gpu::Image all = still(sc);
      frame->options.occlusion = true;
      const CullDiff first = cull_diff(img, all, width);
      lab.metric(name + ".cull-diff-pixels", first.pixels, "count", Det::measured);
      if (first.pixels > 0) {
        const ge::gpu::Image again = still(sc);
        const CullDiff second = cull_diff(again, all, width);
        const ge::render::InstanceStats seen_again =
            ge::render::read_instance_stats(*device, frame->forest);
        const std::string line =
            "world: view '" + name + "' on " + device->report.name +
            ": the culled still differed from the unculled one in " + first.describe() +
            " (trees early+late, hidden: " + std::to_string(trees.early) + "+" +
            std::to_string(trees.late) + ", " + std::to_string(trees.hidden) + "); drawn again, " +
            second.describe() + " (" + std::to_string(seen_again.early) + "+" +
            std::to_string(seen_again.late) + ", " + std::to_string(seen_again.hidden) + ").";
        lab.note(line);
        // The pictures only where a difference fails: an attachment's name is in the digest.
        if (device->report.software) {
          ++cull_faults;
          ge::core::output::error(line);
          lab.attach(name + ".cull-culled.png", ge::lab::encode_png({width, height, img.rgba}),
                     Det::measured);
          lab.attach(name + ".cull-unculled.png", ge::lab::encode_png({width, height, all.rgba}),
                     Det::measured);
          lab.attach(name + ".cull-diff.png",
                     ge::lab::encode_png(diff_picture(img, all, width, height)), Det::measured);
        } else if (second.pixels > 0) {
          ++cull_hardware;
          ge::core::output::warn(line +
                                 " Repeated on hardware: measured, not gating. Skill: render.");
        } else {
          ++cull_unrepeated;
          ge::core::output::warn(line + " Not repeated: not counted as the cull's. Skill: render.");
        }
      }
    }
    ge::lab::Picture picture{width, height, img.rgba};
    if (img.rgba.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
      lab.fail("the readback of view '" + name + "' came back the wrong size",
               "Check engine/gpu/target.cpp's Readback. Skill: gpu.");
      continue;
    }
    lab.shot(shot_of(sc), picture, Det::measured, std::move(distance));
  }
  if (with_console) {
    Scene& sc = scenes.back();
    ge::console::ConsoleView con(console);
    con.add(ge::console::LineKind::output, "Fall Line: seed " + seed_s + ", the free ride");
    con.add(ge::console::LineKind::command, "] /sv_hour 15.5");
    con.add(ge::console::LineKind::event, "a gate missed: 4");
    con.add(ge::console::LineKind::chat, "skier: the powder under the ridge is untracked");
    con.toggle();
    con.frame(10);
    con.type("/r_bl");
    ge::ui::DrawList overlay;
    overlay.begin(width, height);
    ge::ui::draw_console(overlay, con, {}, nullptr, 0);
    fall_line::app::cut(*frame);
    for (int i = 0; i < settle; ++i) {
      sc.view.frame = static_cast<uint64_t>(i);
      fall_line::app::draw(*device, *frame, sc.view,
                           {sc.skiers, 0, true, {}, std::nullopt, &overlay}, target.view);
    }
    const ge::gpu::Image img = ge::gpu::read_now(*device, target);
    if (img.rgba.size() == static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
      lab.shot(console_shot(), ge::lab::Picture{width, height, img.rgba}, Det::measured);
    } else {
      lab.fail("the readback of the console's still came back the wrong size",
               "Check engine/gpu/target.cpp's Readback. Skill: gpu.");
    }
  }
  lab.note(costs);
  lab.metric("overdraw-faults", overdraw_faults, "count");
  if (overdraw_faults != 0) {
    lab.fail(
        "the colour pass missed covered pixels, or shaded them more than once, after the "
        "depth prepass",
        "The prepass and the colour pass must compute the same depth bits: check the "
        "terrain's and the instances' vertex stages (@invariant position, one module each). "
        "Skill: render.");
  }
  lab.metric("cull-faults", cull_faults, "count");
  lab.metric("cull-unrepeated", cull_unrepeated, "count", Det::measured);
  lab.metric("cull-hardware", cull_hardware, "count", Det::measured);
  if (cull_faults != 0) {
    lab.fail("the depth pyramid's cull dropped instances that show (the .cull-diff-pixels rows)",
             "Check engine/render/instances_cull.wgsl's `hidden` (the box's nearest corner, the "
             "mip it reads, the margin) and the late phase. Skill: render.");
  }
  lab.metric("gpu-errors", device->errors.load(), "count");
  lab.metric("pipelines-after-load", device->pipelines_after_load, "count");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors while drawing",
             "Read the gpu: lines above (each names its object's label). Skill: render.");
  }
  if (device->pipelines_after_load != 0) {
    lab.fail("a pipeline was made after the load",
             "Add it to create_frame's manifest. Skill: gpu, § Pipelines.");
  }
}

const ge::lab::LabSpec kSpec{
    "world",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "games/fall_line/app (the frame, its pass table, its views) over engine/render",
    "render",
    "What does Fall Line's frame draw from each named view of a seed's map?",
    {
        {"seed", "38", "the map's seed"},
        {"views", "all", "the views to shoot, comma-separated (games/fall_line/app/views.hpp)"},
        {"hour", "-1", "the solar hour to light the map at; negative: the map's own"},
        {"day", "0", "the day of the year the map is lit on (its moon with it); 0: the map's own"},
        {"cfg", "",
         "console commands over Fall Line's cvars (\"sv_weather storm; sv_cover 0.6\"): "
         "the sky's sv_ cvars and r_clouds reach the map and the picture"},
        {"width", "640", "the picture's width, px"},
        {"height", "360", "the picture's height, px"},
        {"settle", "8", "frames drawn after each view's cut before the capture"},
        {"taa", "1", "TAA-U on (1) or off (0)"},
        {"scale", "1", "the render size over the picture's, with TAA (0.25..1)"},
        {"snow", "1", "the snow's material (1) or the albedo lit (0)"},
        {"trails", "1", "the tracks (1) or none (0); drawn only with the snow's material"},
        {"reach", "0",
         "how far the picture draws, misted into the sky from half of it, m (0: all)"},
        {"moments", "3,15,30",
         "seconds of the bot's run to shoot from the chase camera, comma-"
         "separated (empty: none)"},
        {"rivals", "3", "the bots racing beside him"},
        {"at", "0",
         "the sky's moment each named view is shot at, s; comma-separated for a sequence of "
         "each (the cloud carried on by the wind between them)"},
        {"console", "1", "the last scene again with the drop-down console down over it (0: not)"},
    },
    "--views=start,vista --moments=4 --width=160 --height=90 --settle=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
