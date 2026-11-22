// The bot client's window loop (bot_client.hpp): the team started, ticked on the wall clock, its
// map and beliefs drawn, its video written, the team stopped.
//
// ge:summary The bot client's loop: the team ticked, its world drawn from above, its video written
// ge:keywords bot-client bots-view team-view bot-team bot-camera seconds
// ge:skill bots render
#include "shooters_edge/client/bot_client.hpp"

#include <string>

#include "core/output.hpp"
#include "platform/window.hpp"
#include "shooters_edge/client/bot_view.hpp"
#include "shooters_edge/client/title.hpp"
#include "shooters_edge/net/qw_bot_team.hpp"
#include "shooters_edge/net/qw_team_areas.hpp"
#include "shooters_edge/net/qw_team_view.hpp"
#include "ui/console.hpp"

namespace shooters_edge::client {

namespace {
namespace cl = shooters_edge::client;
namespace out = ge::core::output;
}  // namespace

int run_bot_client(BotClientRun& r) {
  using CArgs = ge::console::Args;
  const Args& args = r.args;
  ge::platform::Window& window = r.window;
  ge::platform::Devices& devices = r.devices;
  ge::platform::InputSnapshot& input = r.input;
  ge::input::Queue& queue = r.queue;
  ge::input::InputState& held = r.held;
  ge::console::Console& console = r.console;
  ge::console::ConsoleView& con_view = r.con_view;
  ge::gpu::Device& device = r.device;
  ge::gpu::Surface& surface = r.surface;
  WorldDraw& title = r.title;
  const SearchPath& files = r.files;
  ge::ui::DrawList& overlay = r.overlay;
  int& w = r.w;
  int& h = r.h;
  const bool& quit = r.quit;
  bool& text_on = r.text_on;
  ge::ui::ConsoleKeys& con_keys = r.con_keys;
  const look::Picture& picture = r.picture;
  const menu::ClassicConsole& con_look = r.con_look;
  ge::render::ScreenshotCopy& shot_copy = r.shot_copy;
  ge::capture::Screenshots& shots = r.shots;
  const float px_per_unit = r.px_per_unit;
  std::string error;
  const sn::BotTeamConfig bc = cl::bot_team_config(args);
  sn::BotTeam bot_team;
  if (!sn::bot_team_start(bot_team, bc, error)) {
    out::error("shooters_edge: the bot client could not start its team (--bots=" +
               std::to_string(args.bots) + " --server=" + args.server + "): " + error +
               "; next: check the server answers (sebots --server=" + args.server +
               " --bots=1), then try again");
    return 1;
  }
  console.set_unknown([&bot_team](const CArgs& a) {
    sn::bot_team_command(bot_team, a.raw);
    return true;
  });
  ge::platform::set_relative_mouse(window, false);
  // The map drawn from above (client/bot_view.hpp) once the team has loaded it; the flat
  // picture (net/qw_team_view.hpp) until then, and for the recording.
  sn::TeamView bot_flat;
  sn::TeamAreas bot_areas;
  cl::BotCam bot_cam;
  cl::WorldDrawPtr bot_world;
  std::string bot_world_map;
  ge::ui::Image bot_image;
  sn::TeamViewVideo bot_video;
  if (!args.bots_view.empty() &&
      !sn::team_view_video_open(bot_video, args.bots_view, w & ~1, h & ~1, 10, error)) {
    out::error("shooters_edge: the team view's video " + args.bots_view + " did not open: " +
               error + "; next: pass --bots-view=<a .mp4 in a folder that exists>");
    return 1;
  }
  const uint64_t bots_from = ge::platform::now_ns();
  uint64_t bots_last = bots_from;
  int64_t next_tick = 0;
  double next_picture = 0;
  bool shot_asked = false;
  while (!quit) {
    queue.clear();
    ge::platform::poll_input(window, input, devices, queue);
    if (input.quit) break;
    const uint64_t now = ge::platform::now_ns();
    const double dt = static_cast<double>(now - bots_last) * 1e-9;
    const double t = static_cast<double>(now - bots_from) * 1e-9;
    bots_last = now;
    if (input.resized) {
      w = input.width_px;
      h = input.height_px;
      surface.configure(device, static_cast<uint32_t>(w), static_cast<uint32_t>(h), true);
      cl::resize_world(device, title, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
      if (bot_world)
        cl::resize_world(device, *bot_world, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    }
    ge::input::update(held, queue);
    ge::ui::feed_console(console, con_view, queue, con_keys);
    console.run();
    if (con_view.typing() != text_on) {
      text_on = con_view.typing();
      ge::platform::set_text_input(window, text_on);
    }
    // The bots' ticks on the wall clock, 13 ms each; a stall drops ticks, never floods them.
    const auto now_us = static_cast<int64_t>((now - bots_from) / 1000);
    for (int k = 0; next_tick <= now_us && k < 8; k++, next_tick += 13000)
      sn::bot_team_tick(bot_team, next_tick);
    if (now_us - next_tick > 200000) next_tick = now_us;
    for (; bot_team.log_read < bot_team.log.size(); bot_team.log_read++)
      console.print(bot_team.log[bot_team.log_read]);
    sn::team_areas_update(bot_areas, bot_team);
    // The map's world, made once the team is on it (again on a new map).
    if (bot_team.loaded && bot_world_map != bot_team.map && !bot_team.seats.empty()) {
      bot_world = cl::create_world_draw(device, files, *bot_team.loaded, static_cast<uint32_t>(w),
                                        static_cast<uint32_t>(h), surface.format(), picture, {},
                                        error, {bot_team.seats[0]->c.models, {}, {}});
      if (!bot_world)
        out::error("shooters_edge: the bots' map " + bot_team.map + " did not draw: " + error +
                   "; next: the flat team view stands in; check the packs under -basedir");
      bot_world_map = bot_team.map;
      bot_cam.fitted = false;
    }
    if (bot_video.open && t >= next_picture) {
      next_picture += 0.1;
      if (!sn::team_view_video_frame(bot_video, sn::team_view(bot_flat, bot_team, w & ~1, h & ~1),
                                     error))
        out::error(error);
    }
    overlay.begin(w, h);
    ge::render::FrameView view = cl::title_view(t);
    if (bot_world) {
      cl::bot_cam_input(bot_cam, bot_team, input, !con_view.open(), dt, h, px_per_unit);
      view = cl::bot_cam_view(bot_cam, bot_team, w, h, t);
      cl::draw_bot_view(overlay, view, w, h, bot_team, bot_areas);
    } else {
      ge::lab::Picture pic = sn::team_view(bot_flat, bot_team, w, h);
      bot_image.width = pic.width;
      bot_image.height = pic.height;
      bot_image.rgba = std::move(pic.rgba);
      bot_image.revision++;
      ge::ui::Box b;
      b.rect = {0, 0, static_cast<float>(w), static_cast<float>(h)};
      b.image = &bot_image;
      b.uv = {0, 0, static_cast<float>(bot_image.width), static_cast<float>(bot_image.height)};
      b.feather = 0;
      overlay.box(b);
    }
    con_view.frame(dt);
    menu::draw_game_console(overlay, con_view, con_look, t);
    const wgpu::TextureView target = r.acquire();
    if (!target) continue;
    cl::draw_world(device, bot_world ? *bot_world : title, view, {}, nullptr, dt, overlay, target);
    shot_copy.frame(device, surface.frame_target(), shots, console);
    r.present();
    // `--seconds=N`: a screenshot a second before the end, then quit (a run that leaves a
    // picture behind).
    if (args.seconds > 0 && t >= args.seconds - 1 && !shot_asked) {
      shot_asked = true;
      console.execute("screenshot");
    }
    if (args.seconds > 0 && t >= args.seconds) break;
  }
  if (!sn::team_view_video_close(bot_video, error)) out::error(error);
  if (!sn::bot_team_stop(bot_team, error)) out::error(error);
  return 0;
}

}  // namespace shooters_edge::client
