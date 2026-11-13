// LAB `se-attract`: A CLASSIC GAME'S MAIN MENU OVER ITS OWN RECORDING, drawn as the window draws it
// with no window. Once a classic game is loaded, the window plays the game's recordings behind the
// main menu (`menu_attract`, client/main.cpp) in place of the title's night, and the menu, the
// console and the pages under the menu are drawn in the game's own pictures and letters
// (menu/classic_view.hpp, menu/classic_pages.hpp, menu/classic_console.hpp). The lab plays the
// recording (`--demo`, demo1) to `--frame` and draws over that frame, each a picture:
//
//   menu      the classic root, SELECT GAME first, its focus
//   games     SELECT GAME: the games by their names' pictures, the one playing, its edition
//   watched   ESCAPE: the menu closed, the recording watched with its play bar
//   console   the console down, in the game's glyph sheet over the game's console background
//   demos     DEMOS in the classic look, listing the install's own folder
//   servers   JOIN A GAME in the classic look, four servers answering on the simulated network
//
// By default the game is the fixture: a hall written in code with a recording through it and the
// menu's stand-in pictures in a pack (deterministic metrics, measured pictures). `--basedir` opens
// a player's install (`id1/pak0.pak`) and plays its own recording: those pictures are the player's
// pack's and stay on this machine and on a private page.
//
// ge:summary Lab: the classic main menu, SELECT GAME, the console and pages over a played demo
// ge:keywords lab se-attract attract demo-loop menu_attract main-menu select-game classic console
// ge:keywords conback conchars demos server-browser playback watched
// ge:skill ui shooter labs
// ge:test tests/shooters_edge_menu_test.cpp
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/classic_pictures.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_playback.hpp"
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/installs.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/menu/carved_letters.hpp"
#include "shooters_edge/menu/classic_console.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "ui/raster.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace sm = shooters_edge::menu;
namespace se = shooters_edge::hud;
namespace gn = ge::net;
namespace ls = ge::lab::liquid_scene;

// The fixture: a hall, a walk through it recorded, and the menu's stand-in pictures.
bool fixture_pack(std::vector<uint8_t>& out, std::string& error) {
  std::vector<uint8_t> bsp;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error))
    return false;
  const cl::DemoScript s = cl::loop_script("halls", {256, 192, 24}, 120, 600);
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
      {"maps/halls.bsp", bsp}, {"demo1.dem", cl::write_single_demo(s)}};
  for (auto& [path, bytes] : sm::fixture_files()) files.push_back({path, bytes});
  return ge::vfs::write_pak(files, &out, &error);
}

// Four servers on the simulated network, each answering a status query with its line.
constexpr const char* kServers[] = {"!Hall of Echoes|dm4|ffa|6|16", "!duel night|dm6|duel|2|2",
                                    "!Saturday 4on4|e1m2|team|8|8", "!coop keep|start|coop|1|4"};
size_t toy_status(uint8_t* out, size_t cap) {
  if (cap < 1) return 0;
  out[0] = '?';
  return 1;
}
bool toy_reply(const uint8_t* data, size_t size, gn::ServerInfo& out) {
  if (size < 2 || data[0] != '!') return false;
  const std::string s(reinterpret_cast<const char*>(data) + 1, size - 1);
  std::vector<std::string> f;
  for (size_t at = 0; at <= s.size();) {
    const size_t end = std::min(s.find('|', at), s.size());
    f.push_back(s.substr(at, end - at));
    at = end + 1;
  }
  if (f.size() != 5) return false;
  out = {};
  out.name = f[0];
  out.map = f[1];
  out.mode = f[2];
  out.players = std::stoi(f[3]);
  out.max_players = std::stoi(f[4]);
  return true;
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 1280;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 800;
  const std::string basedir = lab.arg("basedir");
  const bool real = !basedir.empty();
  std::string error;
  cl::SearchPath files;
  const ge::classicenhance::Profile* content = &ge::classicenhance::classic_profile();
  std::string install;
  if (!real) {
    std::vector<uint8_t> pak;
    if (!fixture_pack(pak, error) || !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
  } else {
    content = cl::install_of(basedir, &install, &error);
    if (content == nullptr ||
        !cl::open_game_dir(install, cl::game_of(install, *content), files, error, *content)) {
      lab.fail(error, "Pass --basedir=<the folder your game is installed in>. Skill: shooter.");
      return;
    }
  }
  const shooters_edge::profiles::GameProfile* row = shooters_edge::profiles::find_game(content->id);
  const cl::GameAdapter& adapter =
      cl::adapter_of(row != nullptr ? *row : shooters_edge::profiles::classic_game());
  const gl::Det det = real ? gl::Det::measured : gl::Det::deterministic;

  // The window's menu and console, in the game's pictures.
  ge::console::Console con;
  for (const char* c : {"map", "sv", "quit", "connect", "playdemo", "toggleconsole", "newgame",
                        "load", "save", "basedir"})
    con.add_command(c, "", [](ge::console::Console&, const ge::console::Args&) {});
  ge::console::ConsoleView con_view(con);
  con.set_sink(
      [&con_view](std::string_view line) { con_view.add(ge::console::LineKind::output, line); });
  sm::GameMenu menu;
  menu.add_cvars(con);
  cl::ClassicPictures pics = cl::load_classic_pictures(files, false, {}, &adapter);
  menu.set_art(std::move(pics.menu));
  se::HudArt hud_art = std::move(pics.hud);
  hud_art.finish();
  if (hud_art.bar.empty()) {
    hud_art = se::free_art();
    hud_art.finish();
  }
  sm::ClassicConsole con_look;
  sm::classic_console(menu.art(), con_look);
  lab.metric("classic-menu", menu.classic() ? 1 : 0, "bool", det);
  lab.metric("classic-console", con_look.on() ? 1 : 0, "bool", det);
  lab.metric("console-background", con_look.back != nullptr ? 1 : 0, "bool", det);
  sm::Host host{.console = &con, .maps = {"start", "dm4"}};
  menu.set_game(real ? install : "/games/classic");
  const std::string edition =
      real ? std::string(cl::edition_words(cl::edition_of(install, *content), *content))
           : "Shareware";
  menu.games().set_games(
      {{.id = std::string(content->id),
        .install = real ? install : "/games/classic",
        .edition = edition,
        .playing = true},
       {.id = content->id == ge::classicenhance::classic_profile().id ? "sequel" : "classic",
        .install = "",
        .edition = "",
        .playing = false,
        .missing = false}});
  // DEMOS on the install's own folder; on the fixture an empty folder of the lab's, never the
  // machine's own home (its files would move the picture from one machine to the next).
  std::string game_dir;
  if (real) {
    game_dir = (std::filesystem::path(install) / content->base_dir).generic_string();
  } else {
    const std::filesystem::path empty = "previews/se-attract-player/base";
    std::error_code ec;
    std::filesystem::remove_all(empty.parent_path(), ec);
    std::filesystem::create_directories(empty, ec);
    game_dir = empty.generic_string();
  }
  menu.demos().set_extensions(cl::listed_extensions(adapter.game->demo_extensions));
  menu.demos().set_game_places({{"GAME", game_dir}});
  menu.demos().set_player_places({{"GAME FOLDER", game_dir}});
  // JOIN A GAME on the simulated network.
  gn::SimNetwork net = gn::make_sim_network(1, gn::LinkConditions{});
  std::vector<gn::Transport> servers(std::size(kServers));
  gn::Transport browser;
  for (size_t i = 0; i < servers.size(); i++)
    gn::open_sim_transport(servers[i], net, gn::sim_address(static_cast<uint8_t>(5 + i), 27500));
  gn::open_sim_transport(browser, net, gn::sim_address(1, 27001));
  menu.servers().set_query({toy_status, toy_reply, nullptr, nullptr});
  menu.servers().use_transport(&browser);
  menu.servers().set_lan_ports(27500, 27500);

  const int iw = static_cast<int>(w), ih = static_cast<int>(h);
  int problems = 0;
  // The pages, each a picture taken by `shot` with `behind` (words) under them; `watched`, when
  // set, draws ESCAPE's picture (the recording watched with its play bar).
  using Over = std::function<void(ge::ui::DrawList&)>;
  using Shot = std::function<void(const char*, const std::string&, const std::string&,
                                  const std::string&, const Over&)>;
  const auto pages = [&](const Shot& shot, const std::string& behind, double t,
                         const Over& watched) {
    const std::string behind_line =
        watched ? "Playing the recording behind the menu." : "No recording plays behind the menu.";
    ge::ui::MenuInput none;
    menu.show(host, "main");
    menu.main().focus("games");
    menu.settle();
    shot("menu", "THE GAME'S OWN MENU OVER ITS RECORDING",
         "The main menu in the game's own pictures, " + behind +
             ". SELECT GAME is the first row, cut in the menu's own letters where they were "
             "measured, the spinning cursor beside it.",
         "the darkened menu over what plays behind it; SELECT GAME first",
         [&](ge::ui::DrawList& dl) { menu.draw(dl, iw, ih, t); });
    ge::ui::MenuInput accept;
    accept.accept = true;
    menu.update(accept, false, host, 1 / 60.0);
    shot("games", "SELECT GAME",
         "SELECT GAME in the classic look: each game's name cut from the menu strips' own carved "
         "letters (its picture on other strips), the one playing marked with its edition and "
         "install, the other not found yet.",
         "two names in the menu's carved letters; the first marked playing; the cursor on it",
         [&](ge::ui::DrawList& dl) { menu.draw(dl, iw, ih, t); });
    menu.show(host, "options");
    menu.settle();
    shot("options", "OPTIONS IN THE GAME'S OWN PICTURES",
         "A page under the root: its title the game's own picture (the sequel's banner over the "
         "screen's top), its rows in the game's glyph sheet.",
         "the title picture centred at the top; the rows' labels right-aligned to the cursor",
         [&](ge::ui::DrawList& dl) { menu.draw(dl, iw, ih, t); });
    menu.hide();
    if (watched)
      shot("watched", "ESCAPE: THE RECORDING WATCHED",
           "ESCAPE closes the menu over the recording, which plays on: watched with its play bar.",
           "the play bar along the bottom over the HUD; no menu", watched);
    con.print("]map dm4");
    con.print("Can't find maps/dm4.bsp in this install.");
    con.print(behind_line);
    con.print("]menu_attract");
    con.print("\"menu_attract\" is \"auto\"");
    con_view.set_open(true);
    for (int k = 0; k < 60; k++) con_view.frame(1 / 30.0);
    shot("console", "THE CONSOLE IN THE GAME'S LETTERS",
         "The console down over the recording: its lines in the game's own glyph sheet, over the "
         "game's console background sliding down with it.",
         "the background picture down to the panel's edge; the lines in the game's letters",
         [&](ge::ui::DrawList& dl) { sm::draw_game_console(dl, con_view, con_look, t); });
    con_view.set_open(false);
    for (int k = 0; k < 60; k++) con_view.frame(1 / 30.0);
    menu.show(host, "demos");
    menu.settle();
    shot("demos", "DEMOS IN THE CLASSIC LOOK",
         "DEMOS in the game's own letters: the place and the folder, the folders and the "
         "recordings with their sizes, the cursor, the keys at the foot.",
         "glyph rows right of the plaque; no modern slabs",
         [&](ge::ui::DrawList& dl) { menu.draw(dl, iw, ih, t); });
    menu.show(host, "servers");
    int64_t now = 0;
    uint8_t buf[64];
    gn::Address from;
    for (int i = 0; i < 90; i++) {
      now += 16000;
      gn::sim_advance(net, now);
      for (size_t s = 0; s < servers.size(); s++)
        while (gn::transport_receive(servers[s], buf, sizeof buf, from, now) >= 0) {
          const std::string_view line = kServers[s];
          gn::transport_send(servers[s], from, reinterpret_cast<const uint8_t*>(line.data()),
                             line.size(), now);
        }
      menu.update(none, false, host, 0.016);
    }
    lab.metric("servers-listed", static_cast<double>(menu.servers().rows().size()), "count", det);
    shot("servers", "JOIN A GAME IN THE CLASSIC LOOK",
         "JOIN A GAME in the game's own letters under its multiplayer title: four servers on the "
         "simulated network, the name, map, players and ping, the selected one's address below.",
         "four rows; the cursor on the first; the footer's address",
         [&](ge::ui::DrawList& dl) { menu.draw(dl, iw, ih, t); });
    // Every carved letter on the measured lines: a capital's top and foot (rows 1 and 16, red), a
    // small capital's (rows 3 and 15, green), the 20 px row (blue).
    shot("letters", "EVERY CARVED LETTER ON ITS LINES",
         "A to Z and a to z in the menu's carved letters, four classic px to a pixel: the ones "
         "the strips hold cut from them, the ones they hold in the other size only grown or shrunk "
         "from it, the F from the E and the I, and B X Z, b x z carved after their traced shapes. "
         "Red: a capital's top and foot; green: a small capital's; blue: the row.",
         "every letter between its lines; no stray pixels beside a letter",
         [&](ge::ui::DrawList& dl) {
           constexpr float kZ = 4;
           const std::string_view lines[] = {"ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ",
                                             "abcdefghi", "jklmnopqr", "stuvwxyz"};
           const auto bar = [&](float y, uint32_t rgb) {
             ge::ui::Box b;
             b.rect = {0, y * kZ, static_cast<float>(iw), 1};
             b.fill = ge::ui::solid(ge::ui::hex(rgb));
             b.feather = 0;
             dl.box(b);
           };
           const sm::ClassicArt& art = menu.art();
           for (size_t k = 0; k < std::size(lines); k++) {
             const float top = 4 + 28 * static_cast<float>(k);
             bar(top + 1, 0xff4646), bar(top + 17, 0xff4646);
             bar(top + 3, 0x46e646), bar(top + 16, 0x46e646), bar(top + 20, 0x5a78ff);
             float x = 4;
             for (const char c : lines[k]) {
               const auto it = art.letters.find(c);
               if (it == art.letters.end()) continue;
               ge::ui::Box b;
               b.rect = {x * kZ, top * kZ, art.width_of(it->second) * kZ, sm::kCarvedLine * kZ};
               b.image = &it->second;
               b.uv = {0, 0, static_cast<float>(it->second.width),
                       static_cast<float>(it->second.height)};
               b.nearest = true;
               b.feather = 0;
               dl.box(b);
               x += art.width_of(it->second) + 4;
             }
           }
         });
  };

  // `--demo=none`: the menu over the title's dark, drawn on the CPU (a game whose recordings
  // cannot play yet keeps the title behind its menu).
  if (lab.arg("demo") == "none") {
    const Shot flat = [&](const char* id, const std::string& title, const std::string& caption,
                          const std::string& look_for, const Over& over) {
      ge::ui::DrawList overlay;
      overlay.begin(iw, ih);
      over(overlay);
      for (const std::string& p : menu.problems()) {
        problems++;
        lab.fail(std::string(id) + ": " + p, "Fix the page's classic layout. Skill: ui.");
      }
      ge::ui::Image img = ge::ui::make_image(iw, ih, ge::ui::hex(0x101018));
      ge::ui::render(overlay, img);
      lab.shot({id,
                title,
                caption,
                look_for,
                {{"demo", "none"}, {"pack", real ? "player's" : "fixture"}}},
               gl::Picture{img.width, img.height, std::move(img.rgba)}, det);
    };
    pages(flat, "the title's dark behind it", 3.25, {});
    lab.metric("problems", problems, "count", det);
    return;
  }

  // The recording, played to the frame the pictures are taken on.
  cl::Timedemo bench;
  if (!cl::timedemo_start(files, lab.arg("demo"), bench, error, adapter)) {
    lab.fail(error, "Name a recording the packs hold (--demo=demo1). Skill: shooter.");
    return;
  }
  lab.note("demo: " + bench.found);
  cl::DemoPlayback playback;
  if (!cl::playback_begin(playback, bench, error)) lab.note("no highlights: " + error);
  const int64_t at =
      std::max<int64_t>(cl::kSkipAfterLevel + 2, static_cast<int64_t>(lab.arg_num("frame")));
  int64_t frames = 0;
  std::unique_ptr<cl::LoadedMap> map;
  for (;;) {
    const cl::TimedemoStep st = cl::timedemo_step(bench, error);
    if (st == cl::TimedemoStep::end || st == cl::TimedemoStep::error) break;
    if (st == cl::TimedemoStep::new_level) {
      map = cl::load_named_map(files, bench.demo.state.map, error);
      if (!map) {
        lab.fail("the recording's map " + bench.demo.state.map + ": " + error,
                 "Check the packs hold it. Skill: shooter.");
        return;
      }
    }
    if (++frames >= at && map) break;
  }
  lab.metric("frame", static_cast<double>(frames), "frames", det);
  if (!map) {
    lab.fail("the recording ended before a level was read",
             "Pass a --frame inside the recording. Skill: shooter.");
    return;
  }
  const cl::DemoState& ds = bench.demo.state;

  auto device = ge::gpu::create_device({.label = "se-attract", .validation = 0});
  if (!device) {
    if (ls::gpu_required())
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    ls::placeholders(lab, {"menu", "games", "watched", "console", "demos", "servers", "letters"});
    ls::checked(lab, 0, 0);
    return;
  }
  shooters_edge::look::Picture picture;
  cl::WorldDrawPtr world =
      cl::create_world_draw(*device, files, *map, w, h, wgpu::TextureFormat::RGBA8Unorm, picture,
                            {}, error, {ds.models, {}}, adapter);
  if (!world) {
    lab.fail(error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  const ge::gpu::Target out = ge::gpu::create_target(*device, "se-attract.final", w, h);
  cl::FxState fx_state;
  fx_state.on = cl::fx_switches(picture);
  fx_state.models = &cl::world_models(*world);
  cl::DemoFx demo_fx;
  cl::FxFrame fx;
  se::Hud hud = se::make_hud();
  const double t = 3.25;
  ge::render::FrameView view = cl::demo_view(ds, ge::camera::FirstPersonRow{}.fov_y, t);
  // The effects run up to the picture's moment, so the arrival's flash has faded as in the window.
  for (int k = 0; k <= 234; k++)
    cl::demo_fx_step(ds, demo_fx, map.get(), view, fx_state, k / 72.0, 1.0 / 72, fx);
  // One picture: the recording's frame with its HUD, then what `over` draws on it.
  const Shot shot = [&](const char* id, const std::string& title, const std::string& caption,
                        const std::string& look_for, const Over& over) {
    ge::ui::DrawList overlay;
    overlay.begin(iw, ih);
    se::fill_hud(hud, cl::demo_status(ds, t), hud_art);
    hud.layout.draw(overlay, hud.layout.frame(iw, ih, se::hud_style()), se::hud_style(hud_art));
    over(overlay);
    for (const std::string& p : menu.problems()) {
      problems++;
      lab.fail(std::string(id) + ": " + p, "Fix the page's classic layout. Skill: ui.");
    }
    for (int k = 0; k < 40; k++) {  // the eye's adaptation settles, as in the window
      view.frame++;
      cl::draw_world(*device, *world, view, fx, nullptr, 0, {}, out.view);
    }
    view.frame++;
    cl::draw_world(*device, *world, view, fx, nullptr, 0, overlay, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    lab.shot({id,
              title,
              caption,
              look_for,
              {{"demo", bench.name}, {"map", ds.map}, {"pack", real ? "player's" : "fixture"}}},
             gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
             gl::Det::measured);
  };
  pages(shot,
        "its recording (" + bench.name + " on " + ds.map +
            ") playing behind it in place of the title's night",
        t, [&](ge::ui::DrawList& dl) {
          playback.shown_until = t + 10;
          cl::playback_draw(dl, playback, bench, iw, ih, t);
        });
  lab.metric("problems", problems, "count", det);
  ls::checked(lab, static_cast<int>(device->errors.load()),
              static_cast<int>(device->pipelines_after_load));
}

const gl::LabSpec kSpec{
    "se-attract",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/menu (the classic look's menu, console and pages) over a played recording",
    "ui",
    "Does a loaded classic game show its own menu, console and pages over its own recording, with "
    "SELECT GAME first?",
    {{"size", "1280,800", "the frame's width and height, px"},
     {"basedir", "", "the folder a player's game is installed in: its own pack and recording"},
     {"demo", "demo1", "the recording played behind the menu; none: the title's dark, on the CPU"},
     {"frame", "400", "the recording's frame the pictures are taken on"}},
    "--size=640,400 --frame=120",
    run,
};
GE_LAB(kSpec);

}  // namespace
