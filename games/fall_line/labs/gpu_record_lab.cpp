// LAB `record`: Fall Line's GPU pictures on the play contract (app/play_render.hpp), the ones
// `ge shot --gpu` and `ge record --gpu` take. The bot skis --seed through the play contract (a
// Session stepped on the pilot, as `ge record` plays a log) to --at seconds, and the renderer
// draws that moment from --view in every mode: lit, wireframe, depth and overlay, one renderer
// for all four (the first picture a cut, the rest drawn on from it, as a recording's frames are).
// Then it draws --frames more pictures a frame apart, as a recording at --fps would, and shoots
// the last: the tracks and the spray carried from picture to picture.
//
// What must hold on every row is deterministic (the modes drawn, the pictures' sizes, no GPU
// error); the pixels are measured: the share of the picture each mode lights (the wireframe's
// lines, the depth's ground) and how much of the picture the recording's frames moved. Without an
// adapter it notes that and draws placeholders, so the digest is the same on every row;
// GE_GPU_REQUIRE=1 fails instead. Built only with GE_GPU.
//
// ge:summary Lab: the play contract's GPU pictures in every mode (lit, wireframe, depth, overlay)
// ge:keywords lab record gpu-shot gpu-recording lit wireframe depth overlay renderer play-render
// ge:skill render agent-interface
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "fall_line/app/play_render.hpp"
#include "fall_line/sim/play.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "play/session.hpp"

namespace {

using ge::lab::Det;
namespace gp = ge::play;

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

// The share of the picture's pixels that are not black, %.
double lit_share(const ge::lab::Picture& p) {
  size_t lit = 0, n = 0;
  for (size_t i = 0; i + 3 < p.rgba.size(); i += 4, ++n) {
    if (p.rgba[i] > 8 || p.rgba[i + 1] > 8 || p.rgba[i + 2] > 8) ++lit;
  }
  return n ? 100.0 * static_cast<double>(lit) / static_cast<double>(n) : 0;
}

// The share of pixels where `a` and `b` differ, %.
double differ_share(const ge::lab::Picture& a, const ge::lab::Picture& b) {
  if (a.rgba.size() != b.rgba.size()) return 100;
  size_t d = 0, n = 0;
  for (size_t i = 0; i + 3 < a.rgba.size(); i += 4, ++n) {
    if (std::memcmp(&a.rgba[i], &b.rgba[i], 3) != 0) ++d;
  }
  return n ? 100.0 * static_cast<double>(d) / static_cast<double>(n) : 0;
}

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

void run(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double at = lab.arg_num("at");
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  const int frames = static_cast<int>(lab.arg_num("frames"));
  const double fps = lab.arg_num("fps");
  const std::string view = lab.arg("view");
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || frames < 0 || !(fps > 0)) {
    lab.fail("--width and --height must be 1..4096, --frames at least 0 and --fps over 0",
             "next: pass --width=480 --height=270 --frames=12 --fps=30.");
    return;
  }
  const gp::GameSpec& spec = fall_line::sim::kGame;
  const double hz = spec.hz;
  gp::Setup setup;
  std::string error;
  if (!gp::make_setup(spec, seed, {}, setup, error)) {
    lab.fail("the setup: " + error, "next: pass a --seed the game builds. Skill: sim.");
    return;
  }
  gp::Session s(spec, setup);
  if (!s.ok()) {
    lab.fail("the run did not open: " + s.error(), "next: pick another --seed. Skill: sim.");
    return;
  }
  std::vector<double> input(spec.axes.size(), 0.0);
  auto step_pilot = [&](int64_t until) {
    while (s.steps() < until && !s.done()) {
      s.run().pilot(input);
      s.step(input);
    }
  };
  step_pilot(static_cast<int64_t>(at * hz));
  lab.metric("steps", static_cast<double>(s.steps()), "count");

  const std::string seed_s = std::to_string(static_cast<long long>(seed));
  const std::array<const char*, 4> modes = {"lit", "wireframe", "depth", "overlay"};
  auto shot_of = [&](const std::string& name, const std::string& caption) {
    std::string title = "SEED " + seed_s + ", " + view + " " + name;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return ge::lab::Shot{
        view + "-" + name,
        title,
        caption,
        "lit: the skier on his skis on the snow, the sky at the map's hour; wireframe: the "
        "ground's tiles in distance bands with no lines through the hill in front, the figure "
        "and the trees orange; depth: near white fading to black, the sky black; overlay: the "
        "piste's red line and the yellow gates only where the ground shows them, the player's "
        "green arrow along his way; the recording's last picture with the tracks behind him",
        {{"seed", seed_s},
         {"view", view},
         {"at", fixed(static_cast<double>(s.steps()) / hz, 2) + " s"},
         {"size", std::to_string(width) + "x" + std::to_string(height)}}};
  };

  std::string open_error;
  std::unique_ptr<gp::Renderer> renderer = fall_line::app::kRenderer.open(open_error);
  if (!renderer) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU renderer, and GE_GPU_REQUIRE=1: " + open_error,
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the pictures are placeholders. " + open_error);
    for (const char* m : modes) {
      ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
      ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
      // The digest hashes in call order, as the device's path makes the calls.
      lab.metric(std::string(m) + ".size-faults", 0, "count");
      lab.shot(shot_of(m, "no GPU adapter"), none, Det::measured);
    }
    lab.shot(shot_of("recorded", "no GPU adapter"),
             ge::lab::create_picture(width, height, {40, 40, 48}), Det::measured);
    lab.metric("draw-faults", 0, "count");
    return;
  }

  int draw_faults = 0;
  ge::lab::Picture lit;
  bool cut = true;
  for (const char* m : modes) {
    ge::lab::Picture p;
    std::string caption;
    if (!renderer->draw(s.run(), {view, m, width, height, cut}, p, caption, error)) {
      lab.fail(std::string("the ") + m + " picture: " + error,
               "Read the gpu: lines above; games/fall_line/app/play_render.cpp. Skill: render.");
      ++draw_faults;
      p = ge::lab::create_picture(width, height, {40, 40, 48});
    }
    cut = false;
    lab.metric(std::string(m) + ".size-faults", p.width == width && p.height == height ? 0 : 1,
               "count");
    lab.metric(std::string(m) + ".lit-share", lit_share(p), "%", Det::measured);
    if (std::string(m) == "lit") lit = p;
    lab.shot(shot_of(m, caption), p, Det::measured);
  }

  // A recording's pictures: one every hz / fps steps, each drawn on from the last.
  const auto every = static_cast<int64_t>(std::llround(hz / fps));
  ge::lab::Picture last;
  std::string caption;
  for (int k = 0; k < frames; ++k) {
    step_pilot(s.steps() + std::max<int64_t>(every, 1));
    if (!renderer->draw(s.run(), {view, "lit", width, height, false}, last, caption, error)) {
      lab.fail("a recording's picture: " + error,
               "Read the gpu: lines above; games/fall_line/app/play_render.cpp. Skill: render.");
      ++draw_faults;
      break;
    }
  }
  if (last.rgba.empty()) last = lit;
  lab.metric("recorded.moved-share", differ_share(lit, last), "%", Det::measured);
  lab.shot(shot_of("recorded", caption.empty() ? "the moment again" : caption), last,
           Det::measured);
  lab.metric("draw-faults", draw_faults, "count");
}

const ge::lab::LabSpec kSpec{
    "record",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "games/fall_line/app/play_render (ge shot --gpu, ge record --gpu) over Fall Line's frame",
    "render",
    "Does the play contract's GPU renderer draw a run in every mode, and carry a recording on?",
    {
        {"seed", "38", "the map's seed"},
        {"at", "10", "seconds of the bot's run before the pictures"},
        {"view", "chase", "the renderer's view (play_render.hpp: chase, high, start, ...)"},
        {"width", "480", "the picture's width, px"},
        {"height", "270", "the picture's height, px"},
        {"frames", "12", "the recording's pictures drawn after the four modes"},
        {"fps", "30", "the recording's frame rate"},
    },
    "--width=160 --height=90 --frames=3 --at=4",
    run,
};
GE_LAB(kSpec);

}  // namespace
