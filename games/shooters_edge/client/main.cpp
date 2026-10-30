// SHOOTER'S EDGE'S WINDOW: the game played on one machine. The game folder's packs are the search
// path (files.hpp), the baseline mod is compiled from the sources built into the game and runs as
// a LISTEN SERVER in this process with the player as its client (listen.hpp), and each frame the
// window reads the keys and the raw mouse (controls.hpp), steps the server by the frames the wall
// time makes due, and draws the map from the player's eyes (engine/camera's first-person rig:
// the view height, the stairs smoothed, the landing's dip) with the game's effects (world_fx.hpp:
// rocket trails, explosions and their light, the screen's blend), the weapon in hand (held.hpp:
// switched, kicked and flashing as the mod fires it), the HUD (hud/layout.hpp) and the console.
//
//   shooters_edge [-basedir <dir>] [-game <folder>] [-progs <file>] [-models <dir>] [--frames=N]
//                 [--size=WxH] [+<command> ...]
//
// `-basedir` is the folder that holds the game folder (`id1` unless `-game` names another) with
// its `pak0.pak`, `pak1.pak` ...; a leading `~` is the home folder, and a path with spaces is one
// argument when quoted. `-progs` runs a mod's progs file in place of the baseline mod, and
// `-models` names the held weapons' folder (by default the tree's
// `games/shooters_edge/content/models/view`, from where the game runs or where it was built). The
// `+` words are console lines run at start: `+map dm6` loads that map
// (`map arena` is the built-in test arena); with none, the map `start` when the packs have it,
// else the arena.
//
// THE KEYS: W A S D move, SPACE jumps, the left button fires, 1..8 pick a weapon, the wheel the
// next one; the mouse looks (captured while the window has it; ESCAPE lets it go, a click takes
// it back); the key under ESCAPE drops the console, where `map <name>` changes the map, `sv
// <line>` is a line for the server's console (`sv timelimit 10`), `sensitivity <deg a count>`
// and `m_invert 0|1` set the look, `r_look classic|modern` and the other picture cvars
// (app/look.hpp: `r_fx_lights`, `r_fx_glow`, `r_bloom`, `r_lava_glow`, `r_slime_glow`) the world's
// look, the view-model cvars (view/hand.hpp: `r_drawviewmodel`, `r_viewmodel_fov`, `cl_gunx`,
// `cl_guny`, `cl_gunz`, `cl_gunsway`) the weapon in hand, and `quit` quits.
//
// ge:summary Shooter's Edge's window: the listen server, the keys and mouse, the world and the HUD
// ge:keywords app main window listen-server basedir game-folder plus-commands map console
// ge:keywords relative-mouse mouse-look hud first-person effects r_look quit shooters-edge
// ge:skill platform input camera render ui shooter
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "console/cfg.hpp"
#include "console/view.hpp"
#include "core/output.hpp"
#include "gpu/device.hpp"
#include "gpu/surface.hpp"
#include "input/state.hpp"
#include "platform/devices.hpp"
#include "platform/window.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "ui/console.hpp"
#include "ui/console_keys.hpp"
#include "ui/draw.hpp"

namespace {

namespace out = ge::core::output;
namespace cl = shooters_edge::client;
namespace se = shooters_edge::hud;

void print_line(const out::LogLine& line, void*) {
  std::FILE* to = line.level == out::Level::error ? stderr : stdout;
  std::fprintf(to, "%.*s\n", static_cast<int>(line.message.size()), line.message.data());
}

struct Args {
  std::string basedir = ".";
  std::string game = "id1";
  std::string progs;   // a mod's progs file ("": the baseline mod)
  std::string models;  // the held weapons' folder ("": the tree's)
  long frames = 0;     // 0: until quit
  int width = 1280, height = 720;
  std::vector<std::string> plus;  // the command line from the first `+` word on
};

// `~` or `~/...` as the home folder (a shell leaves a quoted one as it is).
std::string expand_home(const std::string& p) {
  if (p.empty() || p[0] != '~' || (p.size() > 1 && p[1] != '/')) return p;
#ifdef _WIN32
  const char* home = std::getenv("USERPROFILE");  // NOLINT(concurrency-mt-unsafe): at start
#else
  const char* home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe): at start
#endif
  return home == nullptr ? p : std::string(home) + p.substr(1);
}

bool parse(int argc, char** argv, Args& a) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (!a.plus.empty() || (!arg.empty() && arg[0] == '+')) {
      a.plus.emplace_back(arg);
    } else if ((arg == "-basedir" || arg == "--basedir") && i + 1 < argc) {
      a.basedir = expand_home(argv[++i]);
    } else if ((arg == "-progs" || arg == "--progs") && i + 1 < argc) {
      a.progs = expand_home(argv[++i]);
    } else if ((arg == "-models" || arg == "--models") && i + 1 < argc) {
      a.models = expand_home(argv[++i]);
    } else if ((arg == "-game" || arg == "--game") && i + 1 < argc) {
      a.game = argv[++i];
    } else if (arg.rfind("--frames=", 0) == 0) {
      a.frames = std::strtol(argv[i] + 9, nullptr, 10);
    } else if (arg.rfind("--size=", 0) == 0) {
      const std::string wh(arg.substr(7));
      const size_t x = wh.find('x');
      if (x == std::string::npos) return false;
      a.width = std::atoi(wh.substr(0, x).c_str());
      a.height = std::atoi(wh.substr(x + 1).c_str());
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

static int run_main(int argc, char** argv) {
  out::set_output_sink(print_line);
  Args args;
  if (!parse(argc, argv, args)) {
    out::error(
        "shooters_edge: unknown flag. Usage: shooters_edge [-basedir <dir>] [-game <folder>] "
        "[-progs <file>] [-models <dir>] [--frames=N] [--size=WxH] [+<command> ...]; next: rerun "
        "with those flags only.");
    return 2;
  }

  // The console: its lines on the terminal and in its scrollback; the client's commands.
  ge::console::Console console;
  ge::console::ConsoleView con_view(console);
  console.set_sink([&con_view](std::string_view line) {
    con_view.add(ge::console::LineKind::output, line);
    out::status("console: " + std::string(line));
  });
  std::string next_map;
  bool quit = false;
  cl::ControlSettings controls;
  shooters_edge::look::Picture picture;
  cl::Listen listen;
  using CArgs = ge::console::Args;
  using Con = ge::console::Console;
  console.add_command("map", "map <name>: play the map (`map arena` is built in)",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() < 2)
                          c.print(listen.map ? "map: " + listen.map->name : "map: none");
                        else
                          next_map = a[1];
                      });
  console.add_command("sv", "sv <line>: a line for the server's console",
                      [&](Con&, const CArgs& a) {
                        listen.opt.console.push_back(a.rest(0));
                        if (listen.server)
                          shooters_edge::server::server_command(*listen.server, a.rest(0));
                      });
  console.add_command("sensitivity", "sensitivity <degrees a mouse count>",
                      [&](Con& c, const CArgs& a) {
                        if (a.size() > 1) controls.sensitivity = std::atof(a[1].c_str());
                        c.print("sensitivity " + std::to_string(controls.sensitivity));
                      });
  console.add_command("m_invert", "m_invert 0|1: the mouse's pitch upside down",
                      [&](Con&, const CArgs& a) { controls.invert = a.size() > 1 && a[1] == "1"; });
  shooters_edge::look::add_cvars(console, picture);
  shooters_edge::view::ViewSettings view_settings;
  shooters_edge::view::add_view_cvars(console.cvars(), view_settings);
  console.add_command("quit", "quit the game", [&](Con&, const CArgs&) { quit = true; });
  se::Hud hud = se::make_hud();
  se::add_cvars(console, hud);
  se::HudArt art = se::free_art();
  art.finish();
  ge::console::start(console, args.plus);
  console.cvars().apply_latched();

  cl::SearchPath files;
  std::string error;
  if (!cl::open_game_dir(args.basedir, args.game, files, error)) {
    out::error("shooters_edge: the game folder did not open: " + error +
               "; see docs/shooters-edge.md § Playing it");
    if (next_map != "arena") return 2;
  } else {
    out::status("shooters_edge: " + std::to_string(files.archives.size()) + " packs in " +
                files.folder);
  }
  if (next_map.empty()) {
    std::vector<uint8_t> store;
    next_map = cl::find_file(files, "maps/start.bsp", store).empty() ? "arena" : "start";
  }
  ge::progs::Progs progs;
  std::string source;
  if (!(args.progs.empty() ? cl::baseline_progs(progs, error)
                           : shooters_edge::server::load_mod(args.progs, progs, source, error))) {
    out::error("shooters_edge: the mod did not load: " + error +
               "; see docs/shooters-edge.md § Running your own mod");
    return 2;
  }
  std::unique_ptr<cl::LoadedMap> first = cl::load_named_map(files, next_map, error);
  if (!first || !cl::listen_start(listen, progs, std::move(first), {}, error)) {
    out::error("shooters_edge: the first map did not start: " + error +
               "; see docs/shooters-edge.md § Playing it");
    return 2;
  }
  next_map.clear();

  // The held weapons: the folder given, else the tree's from here, else from where it was built.
  std::vector<ge::viewmodel::HeldModel> weapons;
  {
    std::vector<std::string> dirs;
    if (!args.models.empty()) dirs.push_back(args.models);
    dirs.emplace_back("games/shooters_edge/content/models/view");
#ifdef SE_MODELS_DIR
    dirs.emplace_back(SE_MODELS_DIR);
#endif
    std::string why;
    for (const std::string& dir : dirs)
      if (shooters_edge::view::load_weapon_models(dir, weapons, &why)) break;
    if (weapons.empty())
      out::status("shooters_edge: no weapon is drawn: " + why +
                  "; next: pass -models <the tree's games/shooters_edge/content/models/view>");
  }

  auto window = ge::platform::open_window(
      {.title = "Shooter's Edge", .width = args.width, .height = args.height});
  if (!window) return 1;
  auto device = ge::gpu::create_device();
  if (!device) return 1;
  ge::gpu::Surface surface;
  if (!surface.create(*device, window->native())) return 1;
  int w = 0, h = 0;
  window->pixel_size(w, h);
  surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), true);
  cl::WorldDrawPtr world =
      cl::create_world_draw(*device, files, *listen.map, static_cast<uint32_t>(w),
                            static_cast<uint32_t>(h), surface.format(), picture, weapons, error);
  if (!world) {
    out::error("shooters_edge: the world draw did not load: " + error + "; see docs/rendering.md");
    return 1;
  }
  ge::platform::DevicesPtr devices = ge::platform::open_devices();
  if (!devices) return 1;
  ge::platform::set_relative_mouse(*window, true);
  bool captured = true;

  ge::input::Queue queue;
  ge::input::InputState held;
  const ge::input::InputState idle;
  ge::ui::ConsoleKeys con_keys;
  bool text_on = false;
  ge::ui::DrawList overlay;
  ge::platform::InputSnapshot input;
  cl::ViewState shown;
  cl::FxState fx_state;
  cl::FxFrame fx;
  cl::HeldState in_hand;
  size_t printed = 0;
  const uint64_t first_ns = ge::platform::now_ns();
  uint64_t last = first_ns;
  long presented = 0;
  while ((args.frames == 0 || presented < args.frames) && !quit) {
    queue.clear();
    ge::platform::poll_input(*window, input, *devices, queue);
    const uint64_t now = ge::platform::now_ns();
    const double wall = static_cast<double>(now - last) * 1e-9;
    const double t = static_cast<double>(now - first_ns) * 1e-9;
    last = now;
    if (input.quit) break;
    ge::input::update(held, queue);
    ge::ui::feed_console(console, con_view, queue, con_keys);
    if (con_view.open() != text_on) {
      text_on = con_view.open();
      ge::platform::set_text_input(*window, text_on);
    }
    // The mouse is the look's while captured: the console, Escape or the focus lost lets it go,
    // a click in the window takes it again.
    const bool release = con_view.open() || input.focus_lost ||
                         (!con_keys.held && input.tapped(ge::platform::Key::escape));
    if (captured && release) {
      captured = false;
      ge::platform::set_relative_mouse(*window, false);
    } else if (!captured && !con_view.open() && held.mouse.buttons.went_down(0)) {
      captured = true;
      ge::platform::set_relative_mouse(*window, true);
    }
    const ge::input::InputState& played = con_keys.held || !captured ? idle : held;
    if (input.resized) {
      w = input.width_px;
      h = input.height_px;
      surface.configure(*device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), true);
      cl::resize_world(*device, *world, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    }

    // A map asked for (the console's `map`, or the mod's level change at its limits).
    if (next_map.empty() && !listen.pending_map.empty()) next_map = listen.pending_map;
    if (!next_map.empty()) {
      std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(files, next_map, error);
      next_map.clear();
      if (!m) {
        console.print("map: " + error);
        listen.pending_map.clear();
      } else if (!cl::listen_change(listen, std::move(m), error)) {
        out::error("shooters_edge: the map change failed: " + error +
                   "; see docs/shooters-edge.md § Playing it");
        return 2;
      } else {
        world.reset();
        world = cl::create_world_draw(*device, files, *listen.map, static_cast<uint32_t>(w),
                                      static_cast<uint32_t>(h), surface.format(), picture, weapons,
                                      error);
        if (!world) {
          out::error("shooters_edge: the new world draw did not load: " + error +
                     "; see docs/rendering.md");
          return 1;
        }
        shown = {};
        fx_state = {};
        in_hand = {};
      }
    }

    // The player's hands, then the server's frames.
    cl::turn_view(played, controls, listen.view);
    const ge::shooter::MoveCmd cmd = cl::read_controls(played, controls);
    if (cmd.impulse != 0) listen.impulse = cmd.impulse;
    const int frames = cl::listen_advance(listen, wall, cmd);
    for (; printed < listen.prints.size(); printed++) {
      const std::string& line = listen.prints[printed];
      console.print(line);
      hud.notify.push(line, se::hud_style().ink, t);
    }
    // The eyes on the player and what the world throws, then the overlay: the HUD, the console.
    ge::render::FrameView view = cl::frame_view(listen, shown, frames, t, wall);
    view.frame = static_cast<uint64_t>(presented);
    cl::step_fx(listen, shown, fx_state, t, wall, fx);
    const shooters_edge::view::HandFrame hand =
        cl::step_held(listen, shown, in_hand, view_settings, weapons, t, wall);
    if (hand.flashing)
      fx.lights.push_back(shooters_edge::effects::muzzle_light(hand.flash.position));
    overlay.begin(w, h);
    se::fill_hud(hud, cl::status_of(listen, t), art);
    const ge::hud::Frame hf = hud.layout.frame(w, h, se::hud_style());
    hud.layout.draw(overlay, hf, se::hud_style());
    con_view.frame(wall);
    ge::ui::draw_console(overlay, con_view, {}, nullptr, t);

    wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    cl::draw_world(*device, *world, view, fx, &hand, wall, overlay, target);
    surface.present();
    ++presented;
  }
  const double seconds = static_cast<double>(ge::platform::now_ns() - first_ns) * 1e-9;
  char line[160];
  std::snprintf(line, sizeof line,
                "shooters_edge: %ld frames in %.2f s (%.1f ms a frame), %u GPU errors", presented,
                seconds, presented ? seconds * 1e3 / static_cast<double>(presented) : 0.0,
                device->errors.load());
  out::status(line);
  return device->errors.load() == 0 && !device->lost.load() ? 0 : 1;
}

// What nothing else caught ends the program here, with a line that says so, rather than in
// std::terminate's bare abort.
int main(int argc, char** argv) {
  try {
    return run_main(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr,
                 "shooters_edge: stopped by an uncaught exception: %s. This is a bug: run the same "
                 "command under lldb or gdb with a breakpoint on __cxa_throw to see where it "
                 "was thrown.\n",
                 e.what());
  } catch (...) {
    std::fprintf(
        stderr,
        "shooters_edge: stopped by an uncaught exception that is not a std::exception. This "
        "is a bug: run the same command under lldb or gdb with a breakpoint on "
        "__cxa_throw to see where it was thrown.\n");
  }
  return 1;
}
