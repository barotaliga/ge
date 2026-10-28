// LAB `front-door`: Fall Line's menus over the live mountain, photographed through the game's own
// frame. It generates a seed's map, lets the bot ski --at seconds of it, and draws the chase
// camera's picture with the shell's menu over it (games/fall_line/app/menus.hpp, drawn by the
// frame's `ui` row: render/ui_pass.hpp) at each --viewports size: the FRONT DOOR (the title over
// the run behind it, softened by the scrim and the vignette), its OPTIONS page, and the PAUSE page
// over the stopped run, dimmed. Every page of both menus is also laid out at every size and held
// to `layout_problems`, which must be empty.
//
// What must hold on every row is deterministic (the shots taken, the pages, the layout problems,
// no GPU error); pixels are measured. Without an adapter it notes that and pictures placeholders,
// so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: Fall Line's front door, options and pause pages over the live frame, captured
// ge:keywords lab front-door main-menu pause-page options menus overlay ui-pass fall-line
// ge:keywords viewports portrait landscape layout-problems scrim live-scene
// ge:skill ui render labs
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "fall_line/app/frame.hpp"
#include "fall_line/app/menus.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/simulate.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"

namespace {

using fall_line::app::Mode;
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

struct Viewport {
  int w = 0, h = 0;
};

// "1280x720,390x844": every well-formed WxH in the list.
std::vector<Viewport> parse_viewports(const std::string& list) {
  std::vector<Viewport> out;
  size_t at = 0;
  while (at < list.size()) {
    size_t comma = list.find(',', at);
    if (comma == std::string::npos) comma = list.size();
    const std::string one = list.substr(at, comma - at);
    int w = 0, h = 0;
    const char* end = one.data() + one.size();
    const auto [wx, we] = std::from_chars(one.data(), end, w);
    if (we == std::errc{} && wx != end && *wx == 'x') {
      const auto [hx, he] = std::from_chars(wx + 1, end, h);
      if (he == std::errc{} && hx == end && w > 0 && h > 0) out.push_back({w, h});
    }
    at = comma + 1;
  }
  return out;
}

struct Page {
  const char* id;
  const char* title;
  const char* caption;
  const char* expect;
  Mode mode;
  bool options;  // the options page open over it
};

constexpr Page kPages[] = {
    {"front", "THE FRONT DOOR",
     "The front door over the live mountain: the bot's run from the chase camera behind the "
     "title, PLAY focused, the picture softened by a scrim on the column's side and a vignette.",
     "the title large and sharp, PLAY's slab red and the biggest, OPTIONS and QUIT under it; the "
     "mountain readable behind, darker on the column's side; nothing cut off at the edges",
     Mode::front, false},
    {"options", "THE OPTIONS PAGE",
     "The front door's options page: the camera's view, the sound and the master volume, the "
     "values on the right, the focus on the first stop.",
     "every row's words and value on one line, the headings dimmer than the stops, the slider's "
     "track and its knob at 80%",
     Mode::front, true},
    {"pause", "THE PAUSE PAGE",
     "The pause page over the stopped run: the picture dimmed, RESUME focused.",
     "PAUSED over the column, RESUME's slab red; the run behind darker than on the front door",
     Mode::pause, false},
};

void run(ge::lab::Lab& lab) {
  const std::vector<Viewport> viewports = parse_viewports(lab.arg("viewports"));
  if (viewports.empty()) {
    lab.fail("--viewports has no WxH in it", "Pass --viewports=1280x720,390x844. Skill: labs.");
    return;
  }
  const double seed = lab.arg_num("seed");
  const double at_s = lab.arg_num("at");
  const int settle = static_cast<int>(lab.arg_num("settle"));

  fall_line::app::Shell shell = fall_line::app::make_shell();
  size_t problems = 0;
  for (const Viewport& v : viewports) {
    for (const std::string& p : fall_line::app::shell_layout_problems(shell, v.w, v.h)) {
      lab.note(std::to_string(v.w) + "x" + std::to_string(v.h) + " " + p);
      ++problems;
    }
  }
  lab.metric("viewports", static_cast<double>(viewports.size()), "count");
  lab.metric("layout-problems", static_cast<double>(problems), "count");
  if (problems) {
    lab.fail(std::to_string(problems) + " layout problems in the menus (the notes name them)",
             "Fix the page's words in games/fall_line/app/menus.hpp or the style. Skill: ui.");
  }

  auto shot_of = [&](const Page& p, const Viewport& v) {
    const std::string size = std::to_string(v.w) + "x" + std::to_string(v.h);
    return ge::lab::Shot{std::string(p.id) + "-" + size,
                         std::string(p.title) + " AT " + size,
                         p.caption,
                         p.expect,
                         {{"viewport", size},
                          {"seed", std::to_string(static_cast<long long>(seed))},
                          {"at", std::to_string(static_cast<int>(at_s)) + " s"}}};
  };

  auto device = ge::gpu::create_device({.label = "front-door", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders.");
    // The same shots, then the same metrics, as the device path, so the digest is the same.
    for (const Viewport& v : viewports) {
      for (const Page& p : kPages) {
        ge::lab::Picture none = ge::lab::create_picture(v.w, v.h, {40, 40, 48});
        ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
        lab.shot(shot_of(p, v), none, Det::measured);
      }
    }
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    return;
  }
  const auto& r = device->report;
  lab.note("adapter: " + r.name + " / " + r.driver + " / " + r.backend +
           (r.software ? " (software)" : ""));

  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  const auto shared = std::make_shared<const ge::mountain::mapgen::Level>(
      ge::mountain::mapgen::generate_level(seed, opts));
  const ge::mountain::mapgen::Level& level = *shared;

  // The bot's run to --at seconds: the picture behind every page.
  fall_line::sim::Driver drive(shared, 0);
  for (int k = 0; k < static_cast<int>(at_s * 120) && !drive.over(); ++k) {
    drive.step(fall_line::sim::bot_controls(drive.state(), 0, drive.plan()));
  }
  std::vector<ge::skier::SkierState> skiers;
  for (const auto& racer : drive.state().racers) skiers.push_back(racer.skier);
  ge::render::FrameView view;
  fall_line::app::day_view(level, -1, view);
  fall_line::app::chase_view(level, skiers[0], view);
  view.time_s = at_s;

  const Viewport& v0 = viewports.front();
  auto frame =
      fall_line::app::create_frame(*device, level, static_cast<uint32_t>(v0.w),
                                   static_cast<uint32_t>(v0.h), wgpu::TextureFormat::RGBA8Unorm);
  if (!frame) {
    lab.fail("Fall Line's frame did not build", "Read the gpu: lines above. Skill: render.");
    return;
  }
  ge::ui::DrawList overlay;
  for (const Viewport& v : viewports) {
    const auto w = static_cast<uint32_t>(v.w), h = static_cast<uint32_t>(v.h);
    if (frame->output_width != w || frame->output_height != h) {
      fall_line::app::resize(*device, *frame, w, h);
    }
    const ge::gpu::Target target = ge::gpu::create_target(*device, "front-door.final", w, h);
    for (const Page& p : kPages) {
      shell.mode = p.mode;
      ge::ui::Menu& m = *fall_line::app::menu_up(shell);
      m.reset();
      if (p.options) m.open("options");
      m.move_focus(0);
      fall_line::app::cut(*frame);
      for (int i = 0; i < settle; ++i) {
        view.frame = static_cast<uint64_t>(i);
        fall_line::app::draw_shell(shell, overlay, v.w, v.h, 1.0, view.time_s);
        fall_line::app::FrameScene scene{skiers};
        scene.overlay = &overlay;
        fall_line::app::draw(*device, *frame, view, scene, target.view);
      }
      const ge::gpu::Image got = ge::gpu::read_now(*device, target);
      lab.shot(shot_of(p, v), ge::lab::Picture{v.w, v.h, got.rgba}, Det::measured);
    }
  }
  lab.metric("gpu-errors", device->errors.load(), "count");
  lab.metric("pipelines-after-load", device->pipelines_after_load, "count");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors drawing the menus",
             "Read the gpu: lines above. Skill: render.");
  }
}

const ge::lab::LabSpec kSpec{
    "front-door",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "games/fall_line/app/menus.hpp (the shell's pages) over the frame's ui row",
    "ui",
    "Do Fall Line's front door, options and pause pages read over the live mountain at every "
    "screen size?",
    {
        {"viewports", "1280x720,390x844,844x390", "screen sizes, WxH, comma-separated"},
        {"seed", "38", "the map's seed"},
        {"at", "8", "seconds of the bot's run behind the pages"},
        {"settle", "4", "frames drawn before each capture (TAA's history)"},
    },
    "--viewports=320x180 --at=2 --settle=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
