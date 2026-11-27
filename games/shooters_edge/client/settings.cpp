// The window's settings, the user info and the mouse on the binds (settings.hpp).
//
// ge:summary The window's settings kept in config.cfg, the user info sent, the mouse on binds
// ge:keywords settings save_settings send_user_info mouse_to_binds keys_to_menu catalog cvars-txt
// ge:skill console input
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/client/cvar_layers.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <system_error>

#include "console/binds.hpp"
#include "console/catalog.hpp"
#include "console/cfg.hpp"
#include "core/output.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::client {

std::filesystem::path settings_dir(const std::string& basedir, const std::string& game) {
  return std::filesystem::path(basedir.empty() ? "." : basedir) / game / "settings";
}

std::filesystem::path series_settings_file(const std::string& data_root) {
  if (data_root.empty()) return {};  // no data folder: each game's file holds everything
  return std::filesystem::path(data_root) / "settings" / std::string(ge::console::kConfigFile);
}

std::vector<std::filesystem::path> series_settings_files(const std::string& data_root,
                                                         const std::string& placed) {
  std::vector<std::filesystem::path> out;
  if (data_root.empty()) return out;
  out.push_back(series_settings_file(data_root));
  if (placed.empty()) return out;
  const std::filesystem::path own =
      series_settings_file((std::filesystem::path(placed) / "data").string());
  std::error_code ec;
  if (std::filesystem::is_regular_file(own, ec) && !std::filesystem::equivalent(own, out[0], ec))
    out.push_back(own);
  return out;
}

namespace {

bool write_series(const std::filesystem::path& file, const std::string& text) {
  std::error_code ec;
  std::filesystem::create_directories(file.parent_path(), ec);
  std::ofstream out(file, std::ios::binary);
  return out && (out << text);
}

}  // namespace

bool save_settings(ge::console::Console& con, const std::filesystem::path& series) {
  namespace con_ = ge::console;
  const bool split = !series.empty() && con.config_file().empty();
  // The loaded game's own values of common settings go with it; other games' wait in the series'.
  const std::string game = con.cvars().scope();
  const auto mine = [&game](std::string_view s) { return s == game; };
  const auto others = [&game](std::string_view s) { return s != game; };
  const auto outside = [](std::string_view l) { return !is_game_layer(l); };
  const std::string own =
      split ? con.save_text({.binds = false, .layers = is_game_layer, .scopes = mine})
            : con.save_text();
  if (split &&
      !write_series(series, con.save_text({.binds = true, .layers = outside, .scopes = others}))) {
    ge::core::output::status("shooters_edge: the settings were not saved: " + series.string() +
                             " cannot be written; next: check the data folder's permissions");
    return false;
  }
  // The catalog beside it: every setting described, for the player's CLI (console/catalog.hpp).
  if (con.write_file(con_::kConfigFile, own) &&
      con.write_file(con_::kCatalogFile, con_::catalog_text(con.cvars())))
    return true;
  const std::filesystem::path dir =
      con.search_path().empty() ? std::filesystem::path(".") : con.search_path()[0];
  const std::filesystem::path file =
      con.config_file().empty() ? dir / ge::console::kConfigFile : con.config_file();
  ge::core::output::status("shooters_edge: the settings were not saved: " + file.string() +
                           " cannot be written; next: check the game folder's permissions");
  return false;
}

void add_user_info_cvars(ge::console::Console& con, UserInfo& info) {
  {
    // Every classic game's, each with its own default (Quake's here; Quake II says its own).
    ge::console::CvarLayer classic(con.cvars(), kLayerClassic);
    con.cvars().bind({.name = "rate",
                      .fallback = "2500",
                      .flags = ge::console::kArchive | ge::console::kUserInfo,
                      .help = "How many bytes a second a joined server may send you.",
                      .kind = ge::console::CvarKind::integer,
                      .min = 500,
                      .max = 1000000,
                      .values = "500..1000000: that many bytes a second, held to the server's "
                                "own bounds (2500 the default)"},
                     &info.rate);
    con.cvars().bind({.name = "password",
                      .fallback = "",
                      .flags = ge::console::kUserInfo,
                      .help = "The password a closed server asks of its players.",
                      .kind = ge::console::CvarKind::text,
                      .values = "\"\": none (the default); TEXT: sent when you join"},
                     &info.password);
    con.cvars().bind({.name = "msg",
                      .fallback = "1",
                      .flags = ge::console::kArchive | ge::console::kUserInfo,
                      .help = "The least important messages a joined server sends you.",
                      .kind = ge::console::CvarKind::integer,
                      .min = 0,
                      .max = 3,
                      .values = "0: pickups and everything; 1: from medium up, no pickups (the "
                                "default); 2: important ones and chat; 3: chat alone"},
                     &info.msg);
    con.cvars().bind({.name = "noaim",
                      .fallback = "0",
                      .flags = ge::console::kArchive | ge::console::kUserInfo,
                      .help = "Whether you ask a joined QuakeWorld server for no aim help on "
                              "rockets and grenades.",
                      .kind = ge::console::CvarKind::boolean,
                      .values = "0: the server's aim help (the default); 1: none"},
                     &info.noaim);
  }
  ge::console::CvarLayer shell(con.cvars(), kLayerShell);  // client/cvar_layers.hpp
  // Quake's one word for both colours: `color 4` dresses you all in one, `color 4 12` apart.
  con.add_command(
      "color", "color <shirt> [pants]: your colours, 0..13 each (one for both); color: shows them",
      [&info](ge::console::Console& c, const ge::console::Args& a) {
        if (a.size() < 2) {
          c.print("Your shirt is colour " + std::to_string(info.top) + " and your pants colour " +
                  std::to_string(info.bottom) + ".");
          c.print("To change them: color <shirt> [pants], each 0 to 13.");
          return;
        }
        // Each value as Quake keeps it: its low four bits, past 13 held at 13 (16 is 0, 15 is 13).
        const auto colour = [](const std::string& text) {
          return std::to_string(std::min(std::atoi(text.c_str()) & 15, 13));
        };
        c.cvars().set("topcolor", colour(a[1]), ge::console::Source::local);
        c.cvars().set("bottomcolor", colour(a.size() >= 3 ? a[2] : a[1]),
                      ge::console::Source::local);
      });
  for (const auto& [name, field, what] :
       {std::tuple{"topcolor", &info.top,
                   "Your shirt's colour, which a team game sets to the team's."},
        std::tuple{"bottomcolor", &info.bottom,
                   "Your pants' colour, which a team game sets to the team's."}})
    con.cvars().bind({.name = name,
                      .fallback = "0",
                      .flags = ge::console::kArchive | ge::console::kUserInfo,
                      .help = what,
                      .kind = ge::console::CvarKind::integer,
                      .min = 0,
                      .max = 13,
                      .values = "0..13: that colour of the palette's rows (0 the default)"},
                     field);
  con.cvars().bind({.name = "skin",
                    .fallback = "",
                    .flags = ge::console::kArchive | ge::console::kUserInfo,
                    .help = "The skin others see you in: a picture in their skins folder.",
                    .kind = ge::console::CvarKind::text,
                    .values = "\"\": theirs to choose (the default); TEXT: skins/TEXT.pcx"},
                   &info.skin);
  con.cvars().bind({.name = "model",
                    .fallback = "",
                    .flags = ge::console::kArchive | ge::console::kUserInfo,
                    .help = "The figure others see you as: a folder in their players folder.",
                    .kind = ge::console::CvarKind::text,
                    .values = "\"\": the game's own (the default); TEXT: players/TEXT/"},
                   &info.model);
  con.cvars().bind(
      {.name = "cl_speedjump",
       .fallback = "0",
       .flags = ge::console::kArchive | ge::console::kUserInfo,
       .help = "Whether the easy jump strafes for you when you hold forward and jump, at the share "
               "the server's pm_speedjump sets.",
       .kind = ge::console::CvarKind::boolean,
       .values =
           "0: off (the default); 1: the game weaves the hops for speed and the mouse steers"},
      &info.speedjump);
  con.cvars().bind(
      {.name = "cl_speedjump_view",
       .fallback = "0.5",
       .flags = ge::console::kArchive,
       .help = "How much of the easy jump's left and right swing your view shows.",
       .kind = ge::console::CvarKind::number,
       .min = 0,
       .max = 1,
       .values =
           "0: a still view; 0..1: that share; 1: all of it, as a strafer sees (0.5 the default)"},
      &info.speedjump_view);
}

void send_user_info(Listen& l, const ge::console::Console& con, UserInfo& info) {
  if (l.server == nullptr) return;
  const ge::console::Cvar* n = con.cvars().find("name");
  const std::string name = n != nullptr && !n->value.empty() ? n->value : "player";
  // The easy jump's ask again whenever the slot lost it (a rejoin clears the server's user info).
  if (l.server->sv.clients[0].active && l.server->sv.clients[0].speedjump != info.speedjump)
    l.rules->user_info(0, "cl_speedjump", info.speedjump ? "1" : "0");
  // Quake II's rules hold the view to the field of view they are told (q2_view.hpp).
  const bool q2 = l.rules != nullptr && (shooters_edge::server::q2_of(*l.rules) != nullptr ||
                                         shooters_edge::server::native_q2_of(*l.rules) != nullptr);
  const ge::console::Cvar* lens = con.cvars().find("cl_fov");
  const std::string fov = q2 && lens != nullptr ? lens->value : info.sent_fov;
  if (l.server == info.sent_to && name == info.name && info.top == info.sent_top &&
      info.bottom == info.sent_bottom && info.skin == info.sent_skin &&
      info.model == info.sent_model && fov == info.sent_fov)
    return;
  const bool fov_new = l.server != info.sent_to || fov != info.sent_fov;
  info.sent_fov = fov;
  info.sent_to = l.server;
  info.name = name, info.sent_top = info.top, info.sent_bottom = info.bottom;
  info.sent_skin = info.skin;
  const bool model_named = !info.model.empty() || !info.sent_model.empty();  // none: not sent
  info.sent_model = info.model;
  l.opt.player = name;
  namespace sv = shooters_edge::server;
  // A match's server tells every client (ListenHost::set_info); a campaign's rules take it.
  const auto set = [&l](const std::string& key, const std::string& value) {
    if (l.host)
      l.host->set_info(l.host->game, key, value);
    else
      l.rules->user_info(0, key, value);
  };
  set("name", name);
  set("skin", info.skin);
  if (model_named) set("model", info.model);
  if (q2 && fov_new) set("fov", fov);
  if (l.room) return;  // a team game's lobby wears the team's colours (lobby.cpp)
  set("topcolor", std::to_string(info.top));
  set("bottomcolor", std::to_string(info.bottom));
}

void mouse_to_binds(ge::console::Console& con, const ge::input::InputState& in) {
  for (int b = 0; b < 5; b++) {
    const std::string key = "mouse" + std::to_string(b + 1);
    if (in.mouse.buttons.went_down(b)) con.key(key, true);
    if (in.mouse.buttons.went_up(b)) con.key(key, false);
  }
  if (in.mouse.wheel_y != 0) {
    const char* wheel = in.mouse.wheel_y > 0 ? "mwheelup" : "mwheeldown";
    con.key(wheel, true);
    con.key(wheel, false);
  }
  con.run();
}

void keys_to_menu(shooters_edge::menu::GameMenu& menu, shooters_edge::menu::Host& host,
                  const ge::input::Queue& queue, const ge::input::InputState& in) {
  if (!menu.waiting_key()) return;
  for (const ge::input::Event& e : queue.events) {
    if (e.type != ge::input::EventType::key || e.v[0] <= 0.5f || e.aux != 0) continue;
    const std::string_view key = ge::console::usage_key(e.code);
    if (!key.empty()) menu.press_key(key, host);
  }
  for (int b = 0; b < 5; b++)
    if (in.mouse.buttons.went_down(b)) menu.press_key("mouse" + std::to_string(b + 1), host);
  if (in.mouse.wheel_y != 0) menu.press_key(in.mouse.wheel_y > 0 ? "mwheelup" : "mwheeldown", host);
}

}  // namespace shooters_edge::client
