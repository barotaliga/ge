// THE BOT CLIENT (`se --bots=N --server=<host:port> --team=<name>`): a team of bots joins the
// server (net/qw_bot_team.hpp) and the window shows the team's world as its bots believe it: the
// map from above (client/bot_view.hpp) once the team has loaded it, the flat picture
// (net/qw_team_view.hpp) until then and for `--bots-view=<file>`'s video. The console drives
// them: a line no command names goes to every bot, `bot<N> <line>` to one (`bot1 skill expert`,
// `ready`, `status`). `--seconds=N` takes a screenshot a second before the end and quits.
//
// The window's own pieces (its device, surface, console and title world) are lent by the window
// (client/main.cpp), which runs this loop in place of its own and returns what it returns.
//
// ge:summary The bot client's window loop: a team of bots on a server, its map and beliefs shown
// ge:keywords bot-client bots-view team-view bot-team bot-camera seconds window-loop
// ge:skill bots render
#pragma once

#include <functional>

#include "capture/screenshot.hpp"
#include "console/view.hpp"
#include "gpu/device.hpp"
#include "gpu/surface.hpp"
#include "input/state.hpp"
#include "platform/devices.hpp"
#include "platform/window.hpp"
#include "render/screenshot_copy.hpp"
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/menu/classic_console.hpp"
#include "ui/console_keys.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

// What the window lends the loop.
struct BotClientRun {
  const Args& args;
  ge::platform::Window& window;
  ge::platform::Devices& devices;
  ge::platform::InputSnapshot& input;
  ge::input::Queue& queue;
  ge::input::InputState& held;
  ge::console::Console& console;
  ge::console::ConsoleView& con_view;
  ge::ui::ConsoleKeys& con_keys;
  bool& text_on;  // the platform's text input is on
  ge::gpu::Device& device;
  ge::gpu::Surface& surface;
  WorldDraw& title;  // drawn behind the flat picture until the map's world is made
  const SearchPath& files;
  const look::Picture& picture;
  ge::ui::DrawList& overlay;
  const menu::ClassicConsole& con_look;
  ge::render::ScreenshotCopy& shot_copy;
  ge::capture::Screenshots& shots;
  int& w;
  int& h;
  float px_per_unit;
  const bool& quit;
  std::function<wgpu::TextureView()> acquire;  // the frame's target, null when none
  std::function<void()> present;
};

// The bot client's loop until the window closes, `quit` or `--seconds`; the process's exit code.
int run_bot_client(BotClientRun& r);

}  // namespace shooters_edge::client
