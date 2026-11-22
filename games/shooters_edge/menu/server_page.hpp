// OPTIONS ▸ SERVER SETTINGS: the server the player is on, its settings and their values now, the
// ones he may change by a vote marked with an asterisk (engine/server/sheet.hpp's rows, as the
// window keeps them: net/qw_remote_sheet.hpp). Built at run time on the menu model, so both looks
// draw it as they draw every other page.
//
// - THE FIRST PAGE (`server`) holds the match's own settings (the map, the mode, the deathmatch
//   mode, teamplay, the limits), then a row a subpage (PLAYER HELP, RULES, MOVEMENT, VOTES,
//   SERVER...: the sheet's groups) and a last line saying what the asterisk means. A setting the
//   player cannot change is shown dimmed with its value.
// - A setting a player changes AT ONCE (a match mod's own words, before its match) is sent as he
//   turns it.
// - A setting a VOTE decides is only shown turned until the player leaves it: moving to another
//   row, going back a page or closing the menu asks "CALL A VOTE?" on a page of its own (the
//   setting, its value now and the one asked; YES calls the vote, NO puts it back).
// - The page asks the window for the sheet when it opens (`serversheet open`), again twice a
//   second while it is up (`serversheet refresh`), after every change (`serversheet changed`), and
//   lets it go when the player leaves the last of its pages (`serversheet close`). The lines that
//   change a setting go to the server as the player's own (`cmd <line>`).
//
// Quake's pages write their words in capitals, Quake II's in small letters (`lower`).
//
// ge:summary OPTIONS > SERVER SETTINGS: the server's settings, votes marked, a vote asked on leaving
// ge:keywords server-settings menu server-page vote-question call-a-vote asterisk serversheet
// ge:keywords settings-sheet player-help votable browse-settings
// ge:skill ui net
// ge:test tests/shooters_edge_server_page_test.cpp
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "server/sheet.hpp"
#include "ui/menu.hpp"

namespace shooters_edge::menu {

inline constexpr std::string_view kServerPage = "server";
inline constexpr std::string_view kVotePage = "server/vote";

class ServerPage {
 public:
  // Whether `page` is one of its pages.
  static bool owns(std::string_view page);
  // The pages filled from `sheet` (null: none kept), added to `menu` as they are first needed; a
  // vote not yet asked keeps its row turned. `maps`: the maps a map row offers.
  void fill(ge::ui::Menu& menu, const ge::server::SettingsSheet* sheet,
            const std::vector<std::string>& maps, bool lower, bool classic);
  // An event on one of its pages, the lines it sends run on `con`.
  void event(const ge::ui::MenuEvent& e, ge::ui::Menu& menu, ge::console::Console& con);
  // After a frame's presses: the focus left a row turned for a vote (`page_before`, `item_before`
  // where it was), or the player left every page of it: asked or let go.
  void after(ge::ui::Menu& menu, std::string_view page_before, std::string_view item_before,
             ge::console::Console& con);
  // Each frame while open: the sheet asked again every half second.
  void tick(double dt, ge::console::Console& con);
  // The menu closed: a vote not asked is dropped, the sheet let go.
  void leave(ge::console::Console& con);
  bool open() const { return open_; }
  // The change waiting for its question ("" none): the row's name and the value asked.
  const std::string& pending_name() const { return pending_ ? pending_->name : empty_; }
  const std::string& pending_value() const { return pending_ ? pending_->value : empty_; }

 private:
  struct Pending {
    std::string page, name, value, words, from, to;  // from, to: as shown
  };
  void ask(ge::ui::Menu& menu);
  void send(const ge::server::SheetRow& row, const std::string& value, ge::console::Console& con);

  const ge::server::SettingsSheet* sheet_ = nullptr;
  std::map<std::string, std::vector<std::string>> values_;  // each row's choices' values
  std::optional<Pending> pending_;
  bool open_ = false;
  bool lower_ = false;
  double since_ = 0;
  std::string empty_;
};

}  // namespace shooters_edge::menu
