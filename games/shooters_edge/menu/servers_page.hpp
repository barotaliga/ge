// SHOOTER'S EDGE'S SERVER BROWSER (MULTIPLAYER ▸ JOIN A GAME, `menu_servers`): the engine's server
// list (net/server_list.hpp) on a socket of the page's own, drawn in the modern menu's look or the
// loaded game's classic one (`draw_classic`, classic_pages.hpp: its glyph sheet, its title).
//
// - WHERE IT LOOKS: the LAN (`sb_lan` 1: the status query broadcast to the ports the game's
//   servers use), the masters (`sb_masters`, host:port words, and the masters the loaded install
//   lists, `set_masters`), and the player's own servers (`sb_servers`). The game hands it its
//   protocol (`set_query`); with none, the page says so and lists nothing.
// - THE TABLE: one row a server: its name (a padlock's `*` when it wants a password), the mode
//   (ffa, team, duel, coop or a match mod's own word), the map, players over slots, the ping and
//   the skill when the server says it; the selection ringed in the accent. The title row says how
//   many answered, of how many asked, and whether the refresh is still going.
// - THE FOOTER: the selected server's address, its game folder and what the match is doing, the
//   players by name; under it the keys.
// - THE KEYS: UP and DOWN choose, ENTER (or a click) joins (`connect <address>` on the console),
//   LEFT and RIGHT change the sort (ping, players, name, map, mode), R or F5 refresh, ESCAPE goes
//   back to MULTIPLAYER.
// - THE FILTERS, each a cvar kept in the player's config and a key on the page: F hides full
//   servers (`sb_hidefull`), E empty ones (`sb_hideempty`), M steps through the modes the answers
//   name (`sb_mode`, "" every mode), P through the ping bounds 50, 100, 150 and 250 ms
//   (`sb_maxping`, 0 any). The title row says which are on and how many servers they hide; a list
//   they empty says so, not "no servers found".
//
// The clock is the page's own (the frames' dt added up), so the list's pings are in the same
// microseconds as its sends. A lab or a test hands it a transport on the simulated network
// instead of a socket (`use_transport`) and advances the network itself.
//
// ge:summary Shooter's Edge's server browser page: LAN, masters and own servers listed, joined
// ge:keywords server-browser servers page join-a-game menu_servers sb_masters sb_servers sb_lan
// ge:keywords connect refresh sort ping players mode map skill coop shooters-edge
// ge:keywords filter sb_hidefull sb_hideempty sb_mode sb_maxping hide-full hide-empty
// ge:skill ui net
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "net/server_list.hpp"
#include "net/transport.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "ui/draw.hpp"
#include "ui/menu.hpp"
#include "ui/menu_view.hpp"

namespace ge::console {
class Console;
}

namespace shooters_edge::menu {

enum class ServersAction : uint8_t { none, leave, join };

class ServersPage {
 public:
  ServersPage() = default;
  ServersPage(const ServersPage&) = delete;
  ServersPage& operator=(const ServersPage&) = delete;
  ~ServersPage();

  // Registers `sb_lan`, `sb_masters`, `sb_servers` and the filters' cvars on `con`; the filter
  // keys then set the cvars, so a filter the player picks is kept with the config.
  void add_cvars(ge::console::Console& con);
  void set_query(const ge::net::ServerQuery& q) { query_ = q; }
  bool has_query() const { return query_.status != nullptr; }
  // The masters the loaded install lists (host:port), asked beside `sb_masters`.
  void set_masters(std::vector<std::string> masters) { install_masters_ = std::move(masters); }
  // A transport of the caller's (a lab's, on the simulated network) in place of the page's socket.
  void use_transport(ge::net::Transport* t) { borrowed_ = t; }
  // The ports a LAN broadcast goes to (the game's servers' range).
  void set_lan_ports(uint16_t first, uint16_t last) {
    lan_first_ = first;
    lan_last_ = last;
  }

  // The page opened: a refresh started. False with `why` when no socket opened.
  bool open(std::string* why);
  // One frame: `dt` s passed; the list read and its queries sent.
  void frame(double dt);
  ServersAction input(const ge::ui::MenuInput& in, bool refresh);
  // The address to join (after ServersAction::join).
  const std::string& chosen() const { return chosen_; }

  void draw(ge::ui::DrawList& list, int width, int height, const ge::ui::MenuStyle& style);
  // The same in the loaded game's classic look, in its glyph sheet (classic_pages.hpp); `t` blinks
  // the cursor.
  void draw_classic(ge::ui::DrawList& list, const ClassicArt& art, int width, int height, double t);
  const std::vector<std::string>& problems() const { return problems_; }

  const ge::net::ServerList& list() const { return list_; }
  const std::vector<int>& rows() const { return rows_; }
  int selected() const { return selected_; }
  ge::net::ServerSort sort() const { return sort_; }
  int64_t now_us() const { return now_us_; }
  // What the last open said went wrong ("" none): a master that did not resolve, no socket.
  const std::string& note() const { return note_; }
  // The filters as the title row words them ("" none on), and the answered servers they hide.
  std::string filter_words() const;
  int hidden() const { return hidden_; }
  const ge::net::ServerFilter& filter() const { return filter_; }

 private:
  void refresh();
  void build_rows();
  bool filter_key(char letter);
  void set_cvar(std::string_view name, const std::string& value);
  int hit(float x, float y) const;

  ge::net::ServerQuery query_;
  std::vector<std::string> install_masters_;
  ge::net::Transport own_;
  ge::net::Transport* borrowed_ = nullptr;
  ge::net::ServerList list_;
  ge::net::ServerListConfig config_;
  bool inited_ = false;
  uint16_t lan_first_ = 27500, lan_last_ = 27505;
  int64_t now_us_ = 0;
  double carry_us_ = 0;
  std::vector<int> rows_;
  int selected_ = 0;
  int first_ = 0;  // the first row shown
  ge::net::ServerSort sort_ = ge::net::ServerSort::ping;
  std::string chosen_;
  std::string note_;
  std::vector<ge::ui::Rect> cells_;  // the rows drawn last, from `first_`
  std::vector<std::string> problems_;
  bool lan_ = true;                              // sb_lan
  std::string masters_;                          // sb_masters
  std::string servers_;                          // sb_servers
  bool hide_full_ = false, hide_empty_ = false;  // sb_hidefull, sb_hideempty
  std::string mode_;                             // sb_mode
  int32_t max_ping_ = 0;                         // sb_maxping
  ge::console::Console* con_ = nullptr;          // the filter keys set the cvars through it
  ge::net::ServerFilter filter_;
  int hidden_ = 0;
};

}  // namespace shooters_edge::menu
