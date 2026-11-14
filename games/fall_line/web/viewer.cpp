// FALL LINE'S WEB VIEWER (docs/hub.md § Watching in a browser, M56j): a run watched again on a
// page's canvas. The page hands it `--run=<url>`, the run's input log (play/inputs.hpp's text,
// the file the game writes when a run ends and `ge replay` plays); the viewer fetches it, opens
// the log's game from its setup (the mountain grown again from the seed: no map is downloaded)
// and steps the run off the log with the replay's player (replay/player.hpp), so what the page
// shows is the run itself, to the bit, not a recording of it. Each paint draws the game's own
// frame (app/frame.hpp) from the camera `--view` names (`chase` by default; `high`, or one of the
// map's views) onto the canvas.
//
// On the canvas's keys: Space or K pause, Left and Right skip five seconds, Up and Down change the
// speed (`--speed` the first), 1 and 2 the chase and the high camera, Home back to the start.
//
// ge:summary Fall Line's web viewer: a run's input log played again and drawn on the page's canvas
// ge:keywords web viewer fall-line run input-log replay watch-again canvas seek pause speed
// ge:keywords chase high view
// ge:skill net render
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/num_text.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/play_render.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/state.hpp"
#include "gpu/device.hpp"
#include "gpu/surface.hpp"
#include "platform/window.hpp"
#include "play/inputs.hpp"
#include "replay/player.hpp"
#include "web/browser.hpp"
#include "web/viewer_args.hpp"

namespace {

namespace app = fall_line::app;
namespace sim = fall_line::sim;

constexpr const char* kCanvas = "#view";
constexpr int kKey1 = 30, kKey2 = 31, kKeyK = 14, kKeySpace = 44, kKeyHome = 74;
constexpr int kKeyRight = 79, kKeyLeft = 80, kKeyDown = 81, kKeyUp = 82;
constexpr double kSkipSeconds = 5;
constexpr double kMostPaint = 0.25;  // a paint's time at most, s: a stalled tab does not leap
constexpr double kSeekSeconds = 2;   // the run's seconds a paint may step on a seek
constexpr std::array<double, 7> kSpeeds = {0.25, 0.5, 1, 2, 4, 8, 16};

int fail(const std::string& line, const std::string& detail = "") {
  if (!detail.empty()) std::fprintf(stderr, "fall_line_viewer: %s\n", detail.c_str());
  ge::web::say(line);
  return 1;
}

std::string clock_text(double s) {
  const int whole = static_cast<int>(s);
  const int sec = whole % 60;
  return std::to_string(whole / 60) + (sec < 10 ? ":0" : ":") + std::to_string(sec);
}

// The speed one step up or down the ladder from `now`.
double next_speed(double now, int dir) {
  size_t i = 0;
  while (i + 1 < kSpeeds.size() && kSpeeds[i] < now) i++;
  if (dir > 0 && kSpeeds[i] <= now && i + 1 < kSpeeds.size()) i++;
  if (dir < 0 && i > 0) i--;
  return kSpeeds[i];
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string_view> words(argv + 1, argv + argc);
  ge::web::ViewerArgs args;
  std::string error;
  if (!ge::web::parse_viewer_args(words, args, error))
    return fail("The page gave the viewer words it does not know.", error);
  if (args.run.empty())
    return fail("This viewer watches runs. Open a run's page to watch one.",
                "the viewer was not given --run=<url>");

  std::vector<uint8_t> bytes;
  if (!ge::web::fetch_bytes(args.run, bytes, error, "Loading the run..."))
    return fail("The run did not load. Try again in a moment.", error);
  const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  std::optional<ge::play::InputLog> log = ge::play::parse_inputs(text, error);
  if (!log) return fail("This run cannot be played here: its file is damaged.", error);
  if (!ge::play::fits(*log, sim::kGame, error))
    return fail("This run is from another game or another version, so it cannot be played here.",
                error);

  ge::web::say("Growing the mountain...");
  ge::web::next_frame();  // the line shown before the generator holds the page
  ge::replay::Player player(sim::kGame, std::move(*log));
  if (!player.ok()) return fail("This run cannot be played here.", player.error());
  const sim::GameState* s = sim::state_of(player.run());
  if (s == nullptr || !s->level || s->racers.empty())
    return fail("This run cannot be played here.", "the run has no racer on a map");

  ge::web::say("Starting the picture...");
  std::unique_ptr<ge::gpu::Device> device = ge::gpu::create_device();
  if (!device) return fail("This browser cannot draw the game.");
  ge::platform::NativeWindow canvas;
  canvas.kind = ge::platform::NativeWindow::Kind::canvas;
  canvas.canvas = kCanvas;
  ge::gpu::Surface surface;
  if (!surface.create(*device, canvas)) return fail("This browser cannot draw the game.");
  uint32_t w = 0, h = 0;
  ge::web::canvas_size(kCanvas, w, h);
  if (w == 0 || h == 0) return fail("The picture has no room on this page.");
  surface.configure(*device, w, h);
  app::FrameOptions options;
  std::unique_ptr<app::GameFrame> frame =
      app::create_frame(*device, *s->level, w, h, surface.format(), options);
  if (!frame)
    return fail("This browser's graphics cannot draw this mountain yet.",
                "the frame did not build; the gpu: lines above say why");
  app::show_village(*device, *frame, s->village);

  const double hz = sim::kGame.hz;
  const double length_s = static_cast<double>(player.length()) / hz;
  std::string view_name = args.view.empty() ? "chase" : args.view;
  {
    ge::render::FrameView probe;
    std::optional<std::array<double, 3>> subject;
    if (!app::run_camera(*s, view_name, probe, subject))
      return fail("This run has no view called " + view_name + ".", "--view=" + view_name);
  }
  double speed = std::clamp(args.speed, kSpeeds.front(), kSpeeds.back());
  double at_s = 0;  // where the run is to stand
  bool paused = false, cut_next = true;
  std::string said;
  std::vector<ge::skier::SkierState> skiers;
  double last_ms = ge::web::next_frame();
  for (uint64_t drawn = 0;; drawn++) {
    const double now_ms = ge::web::next_frame();
    const double dt = std::min((now_ms - last_ms) / 1000.0, kMostPaint);
    last_ms = now_ms;

    ge::web::PageInput page;
    ge::web::take_input(kCanvas, page);
    const double was_s = at_s;
    for (const int k : page.pressed) {
      if (k == kKeySpace || k == kKeyK) paused = !paused;
      if (k == kKeyRight) at_s += kSkipSeconds;
      if (k == kKeyLeft) at_s -= kSkipSeconds;
      if (k == kKeyHome) at_s = 0;
      if (k == kKeyUp) speed = next_speed(speed, 1);
      if (k == kKeyDown) speed = next_speed(speed, -1);
      if (k == kKey1) view_name = "chase", cut_next = true;
      if (k == kKey2) view_name = "high", cut_next = true;
    }
    if (!paused) at_s += dt * speed;
    at_s = std::clamp(at_s, 0.0, length_s);
    if (at_s < was_s || at_s - was_s > kSkipSeconds - 0.5) {
      app::forget_tracks(*frame);  // a jump: the tracks laid are another moment's
      cut_next = true;
    }
    player.seek(static_cast<int64_t>(at_s * hz));
    player.advance(static_cast<int64_t>(kSeekSeconds * hz));
    s = sim::state_of(player.run());

    const bool over = player.at() >= player.length();
    const std::string line =
        (paused ? "Paused at "
         : over ? "The run is over at "
                : "") +
        clock_text(s->t) + " of " + clock_text(length_s) +
        (speed != 1 ? ", " + ge::core::num::to_fixed(speed, 2) + "x" : "") +
        ". Space pauses, Left and Right skip, Up and Down change the speed, 1 and 2 the camera.";
    if (line != said) ge::web::say(said = line);

    ge::render::FrameView view;
    std::optional<std::array<double, 3>> subject;
    app::run_camera(*s, view_name, view, subject);
    view.frame = drawn;
    skiers.clear();
    for (const auto& racer : s->racers) skiers.push_back(racer.skier);
    if (cut_next) {
      app::cut(*frame);
      cut_next = false;
    }
    const wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    app::draw(*device, *frame, view,
              {skiers, paused ? 0 : dt * speed, true, {}, subject, nullptr, s->t}, target);
    surface.present();
  }
}
