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
//
// The clock is the page's own (the frames' dt added up), so the list's pings are in the same
// microseconds as its sends. A lab or a test hands it a transport on the simulated network
// instead of a socket (`use_transport`) and advances the network itself.
//
// ge:summary Shooter's Edge's server browser page: LAN, masters and own servers listed, joined
// ge:keywords server-browser servers page join-a-game menu_servers sb_masters sb_servers sb_lan
// ge:keywords connect refresh sort ping players mode map skill coop shooters-edge
// ge:skill ui net
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <string>
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

  // Registers `sb_lan`, `sb_masters` and `sb_servers` on `con`.
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

 private:
  void refresh();
  void build_rows();
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
  bool lan_ = true;      // sb_lan
  std::string masters_;  // sb_masters
  std::string servers_;  // sb_servers
};

}  // namespace shooters_edge::menu
