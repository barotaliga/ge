// LAB `front-door`: Fall Line's menus over the live mountain, photographed through the game's own
// frame. It generates a seed's map, lets the bot ski --at seconds of it, and draws the chase
// camera's picture with the shell's menu over it (games/fall_line/app/menus.hpp, drawn by the
// frame's `ui` row: render/ui_pass.hpp) at each --viewports size: the FRONT DOOR (the title over
// the run behind it, softened by the scrim and the vignette), its OPTIONS page, and the PAUSE page
// over the stopped run, dimmed; then the saves (docs/saves.md § Fall Line's saves in the game): the
// run is saved at --saves seconds of it into a store on a simulated disk, each save with the game's
// picture, and the front door shows CONTINUE, the LOAD card lists them with the focused slot's
// picture beside the column, and the pause page's SAVE card offers the slots. Every page of both
// menus is also laid out at every size and held to `layout_problems`, which must be empty.
//
// What must hold on every row is deterministic (the shots taken, the pages, the layout problems,
// no GPU error); pixels are measured. Without an adapter it notes that and pictures placeholders,
// so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: Fall Line's front door, options and pause pages over the live frame, captured
// ge:keywords lab front-door main-menu pause-page options menus overlay ui-pass fall-line
// ge:keywords viewports portrait landscape layout-problems scrim live-scene load-card save-card
// ge:keywords continue save-picture slots
// ge:skill ui render labs
#include <algorithm>
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
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/simulate.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/generate.hpp"
#include "play/saves.hpp"
#include "save/disk.hpp"
#include "save/store.hpp"

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

// "4,12,20": every number in the list, in order.
std::vector<double> seconds_of(const std::string& list) {
  std::vector<double> out;
  size_t at = 0;
  while (at < list.size()) {
    size_t comma = list.find(',', at);
    if (comma == std::string::npos) comma = list.size();
    // strtod, not from_chars: Apple's library has no from_chars for a double.
    const std::string one = list.substr(at, comma - at);
    char* end = nullptr;
    const double v = std::strtod(one.c_str(), &end);
    if (!one.empty() && *end == '\0') out.push_back(v);
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
  const char* opens;  // the page open over the root ("" none)
};

constexpr Page kPages[] = {
    {"front", "THE FRONT DOOR",
     "The front door over the live mountain: the bot's run from the chase camera behind the "
     "title, CONTINUE focused (the run has saves), the picture softened by a scrim on the "
     "column's side and a vignette.",
     "the title large and sharp, CONTINUE's slab red and the biggest, PLAY, LOAD, OPTIONS and "
     "QUIT under it; the "
     "mountain readable behind, darker on the column's side; nothing cut off at the edges",
     Mode::front, ""},
    {"options", "THE OPTIONS PAGE",
     "The front door's options page: the camera's view, the sound and the master volume, the "
     "values on the right, the focus on the first stop.",
     "every row's words and value on one line, the headings dimmer than the stops, the slider's "
     "track and its knob at 80%",
     Mode::front, "options"},
    {"pause", "THE PAUSE PAGE",
     "The pause page over the stopped run: the picture dimmed, RESUME focused.",
     "PAUSED over the column, RESUME's slab red; the run behind darker than on the front door",
     Mode::pause, ""},
    {"load", "THE LOAD CARD",
     "The front door's LOAD card: the run's saves newest first, the focused one's picture (the "
     "piste round the skier when it was saved) large beside the column.",
     "SAVED RUNS over the slots, each line its slot and title; the focused save's picture beside "
     "the column (above it in portrait) with a thin frame; nothing overlaps",
     Mode::front, "load"},
    {"save", "THE SAVE CARD",
     "The pause page's SAVE card: the slots to save into, a used one with its picture, an empty "
     "one marked EMPTY.",
     "SAVE INTO over the slots; the used slots' lines carry their titles, the rest EMPTY; the "
     "focused slot's picture beside the column",
     Mode::pause, "save"},
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

  // The saves the cards show: the bot's run saved at each --saves second into a store on a
  // simulated disk, the way the window saves the player's (games/fall_line/app/main.cpp).
  ge::save::SimDisk save_disk;
  ge::save::StoreSpec save_spec;
  save_spec.game = fall_line::sim::kGame.name;
  ge::save::Store store(save_disk, save_spec);
  std::string setup_text;
  int saved = 0;
  if (!store.open()) {
    lab.fail("the save store did not open: " + store.problem(),
             "See engine/save/store.hpp. Skill: save.");
    return;
  }
  {
    std::string error;
    ge::play::Setup setup;
    std::unique_ptr<ge::play::Run> run;
    if (ge::play::make_setup(fall_line::sim::kGame, seed, {{"rivals", "0"}}, setup, error))
      run = fall_line::sim::kGame.open(setup, error);
    if (!run) {
      lab.fail("the run to save did not open: " + error,
               "See games/fall_line/sim/play.hpp. Skill: sim.");
      return;
    }
    setup_text = ge::play::setup_text(fall_line::sim::kGame, setup);
    std::vector<double> in(fall_line::sim::kGame.axes.size(), 0);
    int64_t step = 0;
    const char* slots[] = {"slot-1", "slot-2", "quick", "auto-1"};
    for (double t : seconds_of(lab.arg("saves"))) {
      for (; step < static_cast<int64_t>(t * fall_line::sim::kGame.hz) && !run->over(); ++step) {
        if (!run->pilot(in)) std::fill(in.begin(), in.end(), 0.0);
        run->step(in);
      }
      if (saved >= static_cast<int>(std::size(slots))) break;
      ge::save::Meta meta;
      meta.title = "Seed " + std::to_string(static_cast<long long>(seed)) + ", " +
                   std::to_string(static_cast<int>(t)) + " s down";
      meta.saved_unix = 1'800'000'000 + static_cast<int64_t>(t);
      std::vector<uint8_t> file;
      if (!ge::play::save_run_pictured(fall_line::sim::kGame, setup, *run, step, meta, file,
                                       error) ||
          !store.write(slots[saved], file)) {
        lab.fail("the run was not saved: " + error + store.problem(),
                 "See engine/play/saves.hpp. Skill: save.");
        return;
      }
      ++saved;
    }
  }
  fall_line::app::Shell shell = fall_line::app::make_shell();
  fall_line::app::set_saves(shell, fall_line::app::rows_of_store(store, setup_text));
  size_t pictured = 0;
  for (const fall_line::app::SaveRow& row : shell.saves) pictured += row.picture.empty() ? 0 : 1;
  lab.metric("saves", static_cast<double>(shell.saves.size()), "count");
  lab.metric("saves-pictured", static_cast<double>(pictured), "count");
  if (pictured != static_cast<size_t>(saved)) {
    lab.fail(std::to_string(saved - static_cast<int>(pictured)) + " of " + std::to_string(saved) +
                 " saves show no picture on the cards",
             "Check save_run_pictured and Store::picture (engine/save/store.hpp). Skill: save.");
  }
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
      if (*p.opens) m.open(p.opens);
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
        {"saves", "4,12,20,28", "seconds of the bot's run each save is taken at (four at most)"},
    },
    "--viewports=320x180 --at=2 --settle=2 --saves=1,2",
    run,
};
GE_LAB(kSpec);

}  // namespace
