// FALL LINE'S APP: the window, the frame loop, the GPU and the sound, put together. Each frame:
// read the input, hand the run clock the frame's wall time (engine/loop/run_clock.hpp: focus
// lost pauses it), take the steps it grants (the bot skis the map's run, each step's events
// routed to the bank: sound/route.hpp), steer the ride bed off the skier, and draw the map's
// frame (frame.hpp) at vsync, the racers drawn where the run has them, from the camera ladder on
// the player (games/fall_line/camera: the chase to start with, C walks tips, helmet, chase, far
// and high by its hand-overs; --view names the rung to start on, or a still view of the map).
//
// THE SHELL (menus.hpp): the app opens on the front door over the live mountain (the bot skis
// the map behind it, from the chase camera, again from the top when he is down); PLAY starts the
// run, Escape on the run pauses it under the pause page, QUIT leaves. Arrows or W and S move,
// Enter or Space press, Escape goes back. The menus are drawn by the frame's 2D pass over the
// picture (frame.hpp's `ui` row). --play starts on the run, with no front door.
//
// THE SOUND: one synth, rendered on the audio device's thread (platform/audio.hpp) and fed from
// this one through its ring. No device (a container, SDL_AUDIO_DRIVER=dummy failing) is not an
// error: the app runs silent. Focus lost fades the buses out with the clock.
//
//   fall_line [--frames=N] [--size=WxH] [--no-vsync] [--seed=N] [--view=NAME] [--hour=H] [--mute]
//             [--play]
//
// C changes the camera on the run.
//
// --frames quits after N presented frames and prints the frame pace (a smoke run under a
// virtual display: xvfb-run -a build/release/games/fall_line/fall_line --frames=60).
// Closing the window, or QUIT on the front door, quits.
//
// ge:summary Fall Line's app: the window, the frame loop on the run clock, the GPU frame
// ge:keywords app main frame-loop window vsync present run-clock frame-pace fall-line
// ge:keywords camera-key camera-ladder front-door pause-page shell
// ge:skill platform gpu render audio
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "audio/synth.hpp"
#include "camera/to_view.hpp"
#include "core/output.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/menus.hpp"
#include "fall_line/app/views.hpp"
#include "fall_line/camera/rungs.hpp"
#include "fall_line/camera/subject.hpp"
#include "fall_line/camera/world.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/simulate.hpp"
#include "fall_line/sound/bank.hpp"
#include "fall_line/sound/mix.hpp"
#include "fall_line/sound/route.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/surface.hpp"
#include "loop/run_clock.hpp"
#include "mountain/mapgen/generate.hpp"
#include "platform/audio.hpp"
#include "platform/window.hpp"

namespace {

namespace out = ge::core::output;

void print_line(const ge::core::output::LogLine& line, void*) {
  std::FILE* to = line.level == ge::core::output::Level::error ? stderr : stdout;
  std::fprintf(to, "%.*s\n", static_cast<int>(line.message.size()), line.message.data());
}

struct Args {
  long frames = 0;  // 0: until quit
  int width = 1280, height = 720;
  bool vsync = true;
  double seed = 38;
  std::string view = "chase";
  double hour = -1;  // the map's own
  bool mute = false;
  bool play = false;  // straight onto the run, no front door
};

// The device's thread renders the synth; everything reaches it through the synth's ring.
void pull(float* out, int frames, void* user) {
  static_cast<ge::audio::Synth*>(user)->render(out, frames);
}

// How long the buses take to fade in or out, s.
constexpr float kFaderGlide = 0.3f;

// The menus' presses from the keyboard: arrows or W and S move, A and D change, Enter or Space
// press, Escape goes back.
ge::ui::MenuInput menu_input(const ge::platform::InputSnapshot& in) {
  using ge::platform::Key;
  ge::ui::MenuInput m;
  m.up = in.tapped(Key::up) || in.tapped(Key::w);
  m.down = in.tapped(Key::down) || in.tapped(Key::s);
  m.left = in.tapped(Key::left) || in.tapped(Key::a);
  m.right = in.tapped(Key::right) || in.tapped(Key::d);
  m.accept = in.tapped(Key::enter) || in.tapped(Key::space);
  m.back = in.tapped(Key::escape);
  return m;
}

bool parse(int argc, char** argv, Args& a) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg.rfind("--frames=", 0) == 0) {
      a.frames = std::strtol(argv[i] + 9, nullptr, 10);
    } else if (arg.rfind("--size=", 0) == 0) {
      char* x = nullptr;
      a.width = static_cast<int>(std::strtol(argv[i] + 7, &x, 10));
      if (!x || *x != 'x') return false;
      a.height = static_cast<int>(std::strtol(x + 1, nullptr, 10));
      if (a.width <= 0 || a.height <= 0) return false;
    } else if (arg == "--no-vsync") {
      a.vsync = false;
    } else if (arg.rfind("--seed=", 0) == 0) {
      a.seed = std::strtod(argv[i] + 7, nullptr);
    } else if (arg.rfind("--view=", 0) == 0) {
      a.view = std::string(arg.substr(7));
    } else if (arg.rfind("--hour=", 0) == 0) {
      a.hour = std::strtod(argv[i] + 7, nullptr);
    } else if (arg == "--mute") {
      a.mute = true;
    } else if (arg == "--play") {
      a.play = true;
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  out::set_output_sink(print_line);
  Args args;
  if (!parse(argc, argv, args)) {
    out::error(
        "fall_line: unknown flag. Usage: fall_line [--frames=N] [--size=WxH] [--no-vsync] "
        "[--seed=N] [--view=tips|helmet|chase|far|high|orbit|start|piste|lookback|vista|aerial] "
        "[--hour=H] [--mute] [--play]; "
        "next: rerun with those flags only.");
    return 2;
  }
  auto window =
      ge::platform::open_window({.title = "Fall Line", .width = args.width, .height = args.height});
  if (!window) return 1;
  auto device = ge::gpu::create_device();
  if (!device) return 1;
  ge::gpu::Surface surface;
  if (!surface.create(*device, window->native())) return 1;
  int w = 0, h = 0;
  window->pixel_size(w, h);
  surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), args.vsync);

  ge::mountain::mapgen::GenerateOptions opts;
  opts.version = fall_line::app::kMapVersion;
  auto shared = std::make_shared<const ge::mountain::mapgen::Level>(
      ge::mountain::mapgen::generate_level(args.seed, opts));
  const ge::mountain::mapgen::Level& level = *shared;
  ge::render::FrameView view;
  fall_line::app::day_view(level, args.hour, view);
  // A rung of the camera ladder follows the run; any other name is a still view of the map.
  fall_line::camera::Rung rung = fall_line::camera::Rung::chase;
  const bool follow = fall_line::camera::rung_named(args.view, rung);
  if (!follow && !fall_line::app::make_view(level, args.view, view)) {
    out::error("fall_line: no view named '" + args.view + "'; see games/fall_line/app/views.hpp.");
    return 2;
  }
  auto frame = fall_line::app::create_frame(*device, level, static_cast<uint32_t>(w),
                                            static_cast<uint32_t>(h), surface.format());
  if (!frame) return 1;

  // The shell (menus.hpp): the front door first, unless --play.
  fall_line::app::Shell shell =
      fall_line::app::make_shell({.rung = rung, .sound = !args.mute},
                                 "seed " + std::to_string(static_cast<long long>(args.seed)));
  if (args.play) shell.mode = fall_line::app::Mode::run;
  ge::ui::DrawList overlay;

  // The run and its sound. The synth outlives the device that pulls it (declared first).
  ge::audio::Synth synth;
  std::optional<fall_line::sim::Driver> run;
  run.emplace(shared, 0);
  fall_line::sound::RunSound sound = fall_line::sound::make_run_sound();
  fall_line::sound::Faders faders;
  faders.on = !args.mute;
  faders.master = static_cast<float>(shell.settings.volume / 100);
  fall_line::sound::apply_mix(synth, faders, 0);
  std::unique_ptr<ge::platform::AudioOut> speaker;
  if (!args.mute) {
    speaker = ge::platform::open_audio({.rate = 48000, .pull = pull, .user = &synth});
    if (!speaker) {
      out::status("fall_line: no audio device; running silent.");
    } else {
      out::status(std::string("fall_line: audio on '") + speaker->driver() + "'");
    }
  }

  ge::loop::RunClock clock = ge::loop::create_run_clock(120);
  ge::platform::InputSnapshot input;
  uint64_t last = ge::platform::now_ns();
  const uint64_t first = last;
  long presented = 0;
  long steps = 0;
  std::vector<ge::skier::SkierState> skiers;
  skiers.reserve(run->state().racers.size());
  bool heard_out = false;
  bool fresh = true;  // the next frame starts the figures settled and cuts the camera
  // The run's camera: the ladder kept warm, walked by C (games/fall_line/camera/rungs.hpp).
  ge::skier::World skier_world = run->state().world();
  ge::camera::World camera_world = fall_line::camera::camera_world(skier_world);
  fall_line::camera::RunCamera camera = fall_line::camera::make_skier_camera(rung, 1);
  // A new run from the top: behind the front door when the bot is down, or PLAY's.
  auto new_run = [&] {
    fall_line::sound::run_sound_silence(sound, synth);
    run.emplace(shared, 0);
    skier_world = run->state().world();
    camera_world = fall_line::camera::camera_world(skier_world);
    camera = fall_line::camera::make_skier_camera(shell.settings.rung, 1);
    fall_line::app::forget_tracks(*frame);
    fall_line::app::cut(*frame);
    heard_out = false;
    fresh = true;
  };
  double last_frame_s = 0;
  while (args.frames == 0 || presented < args.frames) {
    ge::platform::poll_input(*window, input);
    if (input.quit) break;
    const bool paused = shell.mode == fall_line::app::Mode::pause;
    if (input.focus_lost) {
      ge::loop::pause(clock);
      fall_line::sound::Faders hush = faders;
      hush.on = false;
      fall_line::sound::apply_mix(synth, hush, kFaderGlide);
    }
    if (input.focus_gained) {
      if (!paused) ge::loop::resume(clock);
      fall_line::sound::apply_mix(synth, faders, kFaderGlide);
    }
    if (input.resized) {
      surface.configure(*device, static_cast<uint32_t>(input.width_px),
                        static_cast<uint32_t>(input.height_px), args.vsync);
      fall_line::app::resize(*device, *frame, static_cast<uint32_t>(input.width_px),
                             static_cast<uint32_t>(input.height_px));
    }

    // The shell: the menu up takes the presses (on the run, Escape pauses it).
    const fall_line::app::ShellStep said = fall_line::app::step_shell(shell, menu_input(input));
    if (said.quit) break;
    if (said.play) new_run();
    if (said.pause) {
      ge::loop::pause(clock);
      fall_line::sound::run_sound_silence(sound, synth);
    }
    if (said.resume) ge::loop::resume(clock);
    if (said.to_front) {
      ge::loop::resume(clock);
      new_run();
    }
    if (said.settings_changed) {
      faders.on = shell.settings.sound;
      faders.master = static_cast<float>(shell.settings.volume / 100);
      fall_line::sound::apply_mix(synth, faders, kFaderGlide);
      if (fall_line::camera::rung_of(camera.cam) != shell.settings.rung)
        fall_line::camera::change_rung(camera.cam, shell.settings.rung);
    }
    switch (said.cue) {
      case fall_line::app::MenuCue::focus:
        fall_line::sound::play_menu(synth, fall_line::sound::kMenuFocus);
        break;
      case fall_line::app::MenuCue::press:
        fall_line::sound::play_menu(synth, fall_line::sound::kMenuPress);
        break;
      case fall_line::app::MenuCue::page:
        fall_line::sound::play_menu(synth, fall_line::sound::kMenuPage);
        break;
      case fall_line::app::MenuCue::none:
        break;
    }

    const uint64_t now = ge::platform::now_ns();
    const double wall = static_cast<double>(now - last) * 1e-9;
    const int granted = ge::loop::frame(clock, wall);
    steps += granted;
    last = now;
    for (int k = 0; k < granted && !run->over(); ++k) {
      run->step(fall_line::sim::bot_controls(run->state(), 0, run->plan()));
      fall_line::sound::run_sound_step(sound, synth, run->state());
    }
    if (run->over() && !heard_out) {
      fall_line::sound::run_sound_silence(sound, synth);  // the finish rings on, the bed goes
      heard_out = true;
    } else if (granted > 0 && !run->over()) {
      fall_line::sound::run_sound_frame(sound, synth, run->state(),
                                        static_cast<float>(granted) / 120.0f);
    }
    // Behind the front door the bot skis the map again from the top once he is down.
    if (shell.mode == fall_line::app::Mode::front && run->over()) new_run();

    wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    view.time_s = static_cast<double>(now - first) * 1e-9;
    view.frame = static_cast<uint64_t>(presented);
    skiers.clear();
    for (const auto& racer : run->state().racers) skiers.push_back(racer.skier);
    std::optional<std::array<double, 3>> subject;
    if (follow && !skiers.empty()) {
      if (shell.mode == fall_line::app::Mode::run && input.tapped(ge::platform::Key::c)) {
        fall_line::camera::change_rung(
            camera.cam, fall_line::camera::next_rung(fall_line::camera::rung_of(camera.cam)));
        shell.settings.rung = fall_line::camera::rung_of(camera.cam);
      }
      const double dt = fresh ? 1.0 / 60 : view.time_s - last_frame_s;
      ge::camera::to_frame_view(
          fall_line::camera::frame_camera(camera, skiers[0], skier_world, camera_world, dt), view);
      const ge::camera::Vec3 p = fall_line::camera::framed_point(skiers[0]);
      subject = std::array<double, 3>{p.x, p.y, p.z};
    }
    // The menu up over the picture, its animation on the wall clock (it moves while paused).
    fall_line::app::draw_shell(shell, overlay, static_cast<int>(frame->output_width),
                               static_cast<int>(frame->output_height),
                               fresh ? 0 : view.time_s - last_frame_s, view.time_s);
    last_frame_s = view.time_s;
    // The figures' lags run over the steps this frame took; a new run starts them settled.
    fall_line::app::draw(*device, *frame, view,
                         {skiers, static_cast<double>(granted) / 120.0, fresh, {}, subject}, target,
                         &overlay);
    fresh = false;
    surface.present();
    ++presented;
  }
  const double seconds = static_cast<double>(ge::platform::now_ns() - first) * 1e-9;
  char line[160];
  std::snprintf(line, sizeof line,
                "fall_line: %ld frames in %.2f s (%.1f ms a frame), %ld steps, %u GPU errors",
                presented, seconds,
                presented ? seconds * 1e3 / static_cast<double>(presented) : 0.0, steps,
                device->errors.load());
  out::status(line);
  return device->errors.load() == 0 && !device->lost.load() ? 0 : 1;
}
