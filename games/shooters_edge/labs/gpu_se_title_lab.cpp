// LAB `se-title`: THE TITLE THE WINDOW OPENS ON (client/title.hpp), drawn as the window draws it
// with no window: the night over no map, the full moon low in a total eclipse, two decks of cloud
// carried across it. Three frames: the opening, a few seconds on (the cloud has moved, the moon
// stays put) and the frame a map loads behind, with the loading spinner in its corner
// (hud/spinner.hpp). Measured, as an adapter rasterises its own way: the moon's red (the brightest
// patch's red against its green and blue), how dark the sky stays, and how far the cloud moved.
//
// ge:summary Lab: the title's night: the eclipsed moon, the moving cloud, the loading spinner
// ge:keywords lab se-title title blood-moon lunar-eclipse clouds loading spinner main-menu
// ge:skill shooter render labs
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "hud/spinner.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/title.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "ui/draw.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace ls = ge::lab::liquid_scene;

// The frames' moments, s on the window's clock.
constexpr double kOpen = 0.5, kLater = 4.5, kLoading = 8.0;

double luma(const uint8_t* p) { return 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]; }

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  auto device = ge::gpu::create_device({.label = "se-title", .validation = 1});
  if (!device) {
    if (ls::gpu_required())
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    ls::placeholders(lab, {"open", "later", "loading"});
    ls::checked(lab, 0, 0);
    return;
  }
  shooters_edge::look::Picture look;
  look.look = lab.arg("look");
  std::string error;
  cl::WorldDrawPtr title =
      cl::make_title_world(*device, w, h, wgpu::TextureFormat::RGBA8Unorm, look, error);
  if (!title) {
    lab.fail("the title did not load: " + error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target out = ge::gpu::create_target(*device, "se-title.final", w, h);
  const auto frame = [&](double t, bool spinner) {
    ge::ui::DrawList overlay;
    overlay.begin(static_cast<int>(w), static_cast<int>(h));
    const ge::hud::Style st = shooters_edge::hud::hud_style();
    if (spinner)
      ge::hud::draw_spinner(overlay,
                            ge::hud::frame_for(static_cast<int>(w), static_cast<int>(h), st), st, t,
                            "LOADING DM6", 2.4);
    // A few frames before it, as the window draws them: the eye's adaptation settles.
    for (int k = 8; k > 0; --k)
      cl::draw_world(*device, *title, cl::title_view(t - k / 30.0), {}, nullptr, 1 / 30.0, {},
                     out.view);
    cl::draw_world(*device, *title, cl::title_view(t), {}, nullptr, 1 / 30.0, overlay, out.view);
    return ge::gpu::read_now(*device, out);
  };
  const ge::gpu::Image open = frame(kOpen, false);
  const ge::gpu::Image later = frame(kLater, false);
  const ge::gpu::Image loading = frame(kLoading, true);
  const auto shot = [&](const char* id, const char* title_line, const char* caption,
                        const char* look_for, const ge::gpu::Image& img) {
    lab.shot({id, title_line, caption, look_for, {{"size", lab.arg("size")}, {"look", look.look}}},
             gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
             gl::Det::measured);
  };
  shot("open", "THE TITLE",
       "The night the window opens on behind the main menu: the full moon, drawn near and huge, "
       "low in the Earth's shadow, a deep red, the cloud passing in front of it on the wind.",
       "a red disc right of centre, a third of the frame tall, darker at its top; cloud over it",
       open);
  shot("later", "FOUR SECONDS ON",
       "The same night four seconds later: the cloud has drifted a little across the moon, the "
       "moon has not.",
       "the cloud's shapes moved against the first frame; the moon in the same place", later);
  shot("loading", "A MAP LOADING BEHIND IT",
       "The frame a map loads behind: the title, and the spinner in the bottom right corner with "
       "the map's name and the seconds so far.",
       "the ring and its bright arc bottom right; LOADING DM6 and 2.4S beside it", loading);

  // The moon: the brightest patch (the pixels within half a stop of the brightest), its colour.
  double top = 0;
  const size_t n = size_t(w) * h;
  for (size_t i = 0; i < n; ++i) top = std::max(top, luma(&open.rgba[i * 4]));
  double r = 0, g = 0, b = 0, sky = 0;
  size_t lit = 0;
  for (size_t i = 0; i < n; ++i) {
    const uint8_t* p = &open.rgba[i * 4];
    sky += luma(p);
    if (luma(p) < 0.7 * top) continue;
    r += p[0], g += p[1], b += p[2];
    ++lit;
  }
  const double red = lit > 0 ? r / std::max(1.0, 0.5 * (g + b)) : 0;
  double moved = 0;
  for (size_t i = 0; i < n * 4; ++i) moved += std::fabs(double(open.rgba[i]) - later.rgba[i]);
  // Read off the adapter's pixels: shown, not hashed.
  lab.metric("moon.red-over-green-blue", red, "ratio", gl::Det::measured);
  lab.metric("moon.patch", static_cast<double>(lit), "px", gl::Det::measured);
  lab.metric("sky.mean-luma", sky / static_cast<double>(n), "of 255", gl::Det::measured);
  lab.metric("cloud.moved", moved / static_cast<double>(n * 4), "of 255 a channel",
             gl::Det::measured);
  if (red < 1.5)
    lab.fail("the brightest patch is not red (red " + std::to_string(red) +
                 " times its green and blue): the moon is not in the Earth's shadow",
             "Read client/title.cpp's moon and its shadow (kUmbra, kAxisOff). Skill: render.");
  if (moved / static_cast<double>(n * 4) < 0.5)
    lab.fail("the cloud did not move between the frames",
             "Read client/title.cpp's kPace and the wind (title_sky). Skill: render.");
  ls::checked(lab, static_cast<int>(device->errors.load()),
              static_cast<int>(device->pipelines_after_load));
}

const gl::LabSpec kSpec{
    "se-title",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client (the title the window opens on, the loading spinner)",
    "shooter",
    "Does the window open on a night with a blood red moon and moving cloud, and does a map "
    "loading behind it show the spinner?",
    {{"size", "960,540", "the frame's width and height, px"},
     {"look", "modern", "r_look: classic or modern"}},
    "--size=480,270",
    run,
};
GE_LAB(kSpec);

}  // namespace
