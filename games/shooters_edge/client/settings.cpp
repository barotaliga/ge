// The window's settings, the user info and the mouse on the binds (settings.hpp).
//
// ge:summary The window's settings kept in config.cfg, the user info sent, the mouse on binds
// ge:keywords settings save_settings send_user_info mouse_to_binds keys_to_menu
// ge:skill console input
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/settings.hpp"

#include "console/binds.hpp"
#include "console/cfg.hpp"
#include "core/output.hpp"
#include "shooters_edge/server/commands.hpp"

namespace shooters_edge::client {

std::filesystem::path settings_dir(const std::string& basedir, const std::string& game) {
  return std::filesystem::path(basedir.empty() ? "." : basedir) / game / "settings";
}

bool save_settings(ge::console::Console& con) {
  if (con.write_file(ge::console::kConfigFile, con.save_text())) return true;
  const std::filesystem::path dir =
      con.search_path().empty() ? std::filesystem::path(".") : con.search_path()[0];
  ge::core::output::status(
      "shooters_edge: the settings were not saved: " + (dir / ge::console::kConfigFile).string() +
      " cannot be written; next: check the game folder's permissions");
  return false;
}

void add_user_info_cvars(ge::console::Console& con, UserInfo& info) {
  for (const auto& [name, field, what] :
       {std::tuple{"topcolor", &info.top,
                   "your shirt's colour, 0..13 (a team game sets the team's)"},
        std::tuple{"bottomcolor", &info.bottom,
                   "your pants' colour, 0..13 (a team game sets the team's)"}})
    con.cvars().bind({.name = name,
                      .fallback = "0",
                      .flags = ge::console::kArchive | ge::console::kUserInfo,
                      .help = what,
                      .kind = ge::console::CvarKind::integer,
                      .min = 0,
                      .max = 13},
                     field);
}

void send_user_info(Listen& l, const ge::console::Console& con, UserInfo& info) {
  if (l.server == nullptr) return;
  const ge::console::Cvar* n = con.cvars().find("name");
  const std::string name = n != nullptr && !n->value.empty() ? n->value : "player";
  if (l.server == info.sent_to && name == info.name && info.top == info.sent_top &&
      info.bottom == info.sent_bottom)
    return;
  info.sent_to = l.server;
  info.name = name, info.sent_top = info.top, info.sent_bottom = info.bottom;
  l.opt.player = name;
  namespace sv = shooters_edge::server;
  sv::set_user_info(*l.server, 0, "name", name);
  if (l.room) return;  // a team game's lobby wears the team's colours (lobby.cpp)
  sv::set_user_info(*l.server, 0, "topcolor", std::to_string(info.top));
  sv::set_user_info(*l.server, 0, "bottomcolor", std::to_string(info.bottom));
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
