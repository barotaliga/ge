// SHOOTER'S EDGE'S SERVER BROWSER (MULTIPLAYER ▸ JOIN A GAME, `menu_servers`): the engine's server
// list (net/server_list.hpp) on a socket of the page's own, drawn in the modern menu's look or the
// loaded game's classic one (`draw_classic`, classic_pages.hpp: its glyph sheet, its title).
//
// - WHERE IT LOOKS: the LAN (`sb_lan` 1: the status query broadcast to the ports the game's
//   servers use), the masters (`sb_masters`, host:port words, and the masters the loaded install
//   lists, `set_masters`; a web address there is a list over HTTP, `WebList`), the community
//   masters our own servers beat to (`sb_hubmasters`, the engine's masters, net/master.hpp,
//   asked for the query's `protocol`), and the player's own servers (`sb_servers`, and the
//   address book a game keeps, Quake II's `adr0` to `adr15`). The game hands it its protocol
//   (`set_query`); with none, the page says so and lists nothing.
// - THE TABLE: one row a server: its name (a padlock's `*` when it wants a password), the mode
//   (ffa, team, duel, coop or a match mod's own word), the map, players over slots, the ping and
//   the skill when the server says it; the selection ringed in the accent. The title row says how
//   many answered, of how many asked, and whether the refresh is still going.
// - THE FOOTER: the selected server's address, its game folder and what the match is doing, the
//   players by name; under it the keys.
// - THE KEYS: UP and DOWN choose, ENTER (or a click) joins (`connect <address>` on the console;
//   `sb_autohide` 0 keeps the page open), LEFT and RIGHT put the next or last column first in the
//   sort, D turns the sort round, R or F5 refresh, ESCAPE goes back to MULTIPLAYER.
// - THE FILTERS, each a cvar kept in the player's config, the first four a key on the page: F
//   hides full servers (`sb_hidefull`: the players at the slots, watchers not counted), E empty
//   ones (`sb_hideempty`: nobody on, players or watchers), M steps through the modes the answers
//   name (`sb_mode`, "" every mode), P through the ping bounds 50, 100, 150 and 250 ms
//   (`sb_maxping`, 0 any). Then `sb_hidenotempty` (only the empty), `sb_hidedead` (1: the
//   servers that did not answer), `sb_hidehighping` with `sb_pinglimit` (80: above it hidden) and
//   `sb_info_filter`, words on the servers' own info keys (`+map=dm` shows the maps with dm in
//   their name, `-*gamedir=fortress` hides one game; net/server_list.hpp's `parse_info_filter`),
//   a word it cannot read said in the footer. The title row says which are on and how many
//   servers they hide; a list they empty says so, not "no servers found".
// - THE COLOURS (`sb_colorservers` 1; Quake II spells it `ui_colorservers`): a row's words grey
//   when the server wants a password, says `anticheat` 2 or more or `NoFake` ENABLED, green when
//   it answered in under 30 ms, the usual colour otherwise (`row_colour`).
// - THE COLUMNS (`sb_show*`, `sb_status`) and THE SORT (`sb_sortservers`): servers_page.cpp.
// - ROUTES (`sb_findroutes` 1): once the refresh is done, each forwarder listed is asked for the
//   servers it reaches (engine/forward/routes.hpp, `sb_proxinfopersec` 10, `sb_proxretries` 3,
//   `sb_proxtimeout` 1000 ms; `sb_ignore_proxy` words leave forwarders out), and the PING column
//   shows the best summed route; the foot says the route, a join goes through it (`cl_proxyaddr`
//   set to the forwarders, cleared for a straight one), as `connectbr` and `observebr` do on the
//   console. `sb_showproxies` 0 hides the forwarders from the list, 1 shows them, 2 only them.
// - THE QUERIES: `sb_inforetries` (3), `sb_infotimeout` (1000 ms), `sb_infospersec` (100),
//   `sb_masterretries` (3), `sb_mastertimeout` (1000 ms); `sb_liveupdate` (2 s; 0 off) asks the
//   selected server again once the refresh is done; `sb_autoupdate` 0 keeps the last list when the
//   page opens again (R refreshes).
// The clock is the page's own (the frames' dt added up), so the list's pings are in the same
// microseconds as its sends. A lab or a test hands it a transport on the simulated network
// instead of a socket (`use_transport`) and advances the network itself.
//
// ge:summary Shooter's Edge's server browser page: LAN, masters and own servers listed, joined
// ge:keywords server-browser servers page join-a-game menu_servers sb_masters sb_servers sb_lan
// ge:keywords sb_hubmasters hub-master sv-master address-book adr0 web-list use_sources
// ge:keywords favorites broadcast servers-lst q2servers pushmenu-servers
// ge:keywords connect refresh sort ping players mode map skill coop shooters-edge
// ge:keywords filter sb_hidefull sb_hideempty sb_mode sb_maxping hide-full hide-empty
// ge:keywords sb_hidenotempty sb_hidedead sb_hidehighping sb_pinglimit sb_info_filter sb_status
// ge:keywords sb_showaddress sb_showping sb_showgamedir sb_showmap sb_showplayers sb_showfraglimit
// ge:keywords sb_showtimelimit sb_sortservers sb_inforetries sb_infotimeout sb_infospersec
// ge:keywords sb_masterretries sb_mastertimeout sb_liveupdate sb_autohide sb_autoupdate columns
// ge:keywords sb_findroutes sb_showproxies sb_ignore_proxy connectbr observebr cl_proxyaddr routes
// ge:skill ui net
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "forward/routes.hpp"
#include "net/http_fetch.hpp"
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

class ServersPage {  // NOLINT(clang-analyzer-optin.performance.Padding): fields by subject
 public:
  ServersPage() = default;
  ServersPage(const ServersPage&) = delete;
  ServersPage& operator=(const ServersPage&) = delete;
  ~ServersPage();

  // Quake II's browser settings, bound by its layer (client/cvar_layers.cpp) and set aside for
  // every other game: `ui_sortservers` sorts by one column in place of `sb_sortservers`
  // (kNoGameSort off; 0 the order the servers were found; 1 name, 2 game folder, 3 map, 4 players
  // (most first), 5 ping; minus the other way; ties by name the same way, then by address), and
  // `ui_pingrate` paces the asks in place of `sb_infospersec` (-1 off; 0 the player's `rate`
  // / 450, 1..100; 1..100 that many a second).
  static constexpr int32_t kNoGameSort = 99;
  int32_t game_sort = kNoGameSort;
  int32_t game_ping_rate = -1;

  // Registers `sb_lan`, `sb_masters`, `sb_servers` and the filters' cvars on `con`; the filter
  // keys then set the cvars, so a filter the player picks is kept with the config.
  void add_cvars(ge::console::Console& con);
  void set_query(const ge::net::ServerQuery& q) { query_ = q; }
  bool has_query() const { return query_.status != nullptr; }
  // The game's forwarders' protocol (forward/routes.hpp), for `sb_findroutes`.
  void set_routes(const ge::forward::RouteQuery& q) { route_query_ = q; }
  // A transport of the caller's for the forwarders' asks (a lab's), in place of a socket of its
  // own.
  void use_route_transport(ge::net::Transport* t) { route_borrowed_ = t; }
  // The best route found to the list's entry `index` (found false: none, or routes off).
  ge::forward::Route route_to(int index) const;
  bool routes_ready() const { return routes_ready_; }
  bool finds_routes() const { return find_routes_; }
  // `connectbr` and `observebr`, and a join from the page with `sb_findroutes`: through the best
  // route to `address` (cl_proxyaddr set to its forwarders), said on the console.
  void connect_by_route(ge::console::Console& c, const std::string& address, bool observe);
  // The player's folder for the game: `sb_listcache` and `sb_mastercache` keep their files in
  // its sb/ ("" none kept).
  void set_cache_dir(std::filesystem::path dir) { cache_dir_ = std::move(dir); }
  // The masters the loaded install lists (host:port), asked beside `sb_masters`.
  void set_masters(std::vector<std::string> masters) { install_masters_ = std::move(masters); }
  // A transport of the caller's (a lab's, on the simulated network) in place of the page's socket.
  void use_transport(ge::net::Transport* t) { borrowed_ = t; }
  // The ports a LAN broadcast goes to (the game's servers' range).
  void set_lan_ports(uint16_t first, uint16_t last) {
    lan_first_ = first;
    lan_last_ = last;
  }

  // The sources the next open asks in place of the configured ones ("" none: the configured):
  // `http://...` and `+http://...` lists, `favorites://` (the address book), `broadcast://`
  // (the LAN), `file:///<name>` (a list in the game's files, read by `set_files`) and
  // `quake2://<address>`; another word is said and left out. Quake II's `pushmenu servers`.
  void use_sources(std::vector<std::string> words) { sources_ = std::move(words); }
  // How a `file:///` source is read: the file's text by its name in the game's files.
  void set_files(std::function<std::optional<std::string>(std::string_view)> read) {
    read_file_ = std::move(read);
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
  // The sort as the title row words it ("ping, then address").
  std::string sort_words() const;
  // Whether joining from the page closes the menu (`sb_autohide`).
  bool hide_on_join() const { return autohide_ != 0; }
  int64_t now_us() const { return now_us_; }
  // What the last open said went wrong ("" none): a master that did not resolve, no socket.
  const std::string& note() const { return note_; }
  // The filters as the title row words them ("" none on), and the answered servers they hide.
  std::string filter_words() const;
  int hidden() const { return hidden_; }
  const ge::net::ServerFilter& filter() const { return filter_; }
  // sb_colorservers' colour for a server's row (`answered` in `ping_ms`); false: the usual one.
  static bool row_colour(const ge::net::ServerInfo& in, int ping_ms, ge::ui::Color& out);

 private:
  void refresh();
  void build_rows();
  bool filter_key(char letter);
  void sort_step(int dir);
  bool sorted_by(ge::net::ServerColumn c) const;
  int effective_ping() const;  // sb_maxping, or sb_pinglimit when sb_hidehighping is lower
  int32_t ping_rate() const;   // the servers asked a second
  void read_settings();  // the sort's codes and the info filter's words, when their text changed
  void route_frame();
  bool read_list_cache();  // sb_listcache's file into the list; false: none read
  void add_cached();       // the masters' kept lists, asked beside them
  void write_caches();     // once a refresh is done: sb_listcache's file, sb_mastercache's
  void tell_done();        // once a refresh is done: the player's f_sbrefreshdone
  std::filesystem::path master_cache_path(const ge::net::Address& master) const;
  bool proxy_row(const ge::net::ServerInfo& info) const;
  std::string route_words(int index) const;
  std::string ping_words(int index, int ping_ms) const;
  void set_cvar(std::string_view name, const std::string& value);
  int hit(float x, float y) const;

  ge::net::ServerQuery query_;
  std::vector<std::string> install_masters_;
  ge::net::Transport own_;
  ge::net::Transport* borrowed_ = nullptr;
  ge::forward::RouteQuery route_query_;
  ge::net::Transport route_own_;
  ge::net::Transport* route_borrowed_ = nullptr;
  ge::forward::RouteFinder finder_;
  ge::forward::RouteTable table_;
  bool routing_ = false, routes_ready_ = false;
  ge::net::ServerList list_;
  ge::net::ServerListConfig config_;
  bool inited_ = false;
  uint16_t lan_first_ = 27500, lan_last_ = 27505;
  int64_t now_us_ = 0;
  double carry_us_ = 0;
  std::vector<int> rows_;
  int selected_ = 0;
  int first_ = 0;  // the first row shown
  std::vector<ge::net::ServerSortKey> sort_keys_;
  std::string sort_seen_ = "\x01";        // the sb_sortservers text `sort_keys_` was read from
  int32_t game_sort_seen_ = kNoGameSort;  // the game_sort `sort_keys_` was read from
  std::string info_seen_ = "\x01";        // the sb_info_filter text `info_rules_` was read from
  std::vector<ge::net::InfoRule> info_rules_;
  std::string info_problem_;  // the first word the info filter could not read
  double live_s_ = 0;         // since the selected server was last asked
  std::string chosen_;
  std::string note_;
  std::vector<ge::ui::Rect> cells_;  // the rows drawn last, from `first_`
  std::vector<std::string> problems_;
  bool lan_ = true;                   // sb_lan
  std::vector<std::string> sources_;  // use_sources (one open's)
  std::function<std::optional<std::string>(std::string_view)> read_file_;
  std::string masters_;                             // sb_masters
  std::string hub_masters_;                         // sb_hubmasters
  std::string servers_;                             // sb_servers
  bool hide_full_ = false, hide_empty_ = false;     // sb_hidefull, sb_hideempty
  std::string mode_;                                // sb_mode
  int32_t max_ping_ = 0;                            // sb_maxping
  bool hide_not_empty_ = false, hide_dead_ = true;  // sb_hidenotempty, sb_hidedead
  bool hide_high_ = false;                          // sb_hidehighping
  int32_t ping_limit_ = 80;                         // sb_pinglimit
  std::string info_filter_;                         // sb_info_filter
  bool show_address_ = false, show_ping_ = true, show_game_ = false;  // sb_show*
  bool show_map_ = true, show_players_ = true, show_fl_ = false, show_tl_ = false;
  bool status_ = true;                                                  // sb_status
  std::string sort_servers_ = "32";                                     // sb_sortservers
  int32_t info_retries_ = 3, info_timeout_ = 1000, infos_per_s_ = 100;  // sb_info*
  int32_t master_retries_ = 3, master_timeout_ = 1000;                  // sb_master*
  float live_update_ = 2;                                               // sb_liveupdate, s
  int32_t autohide_ = 1;                                                // sb_autohide
  bool autoupdate_ = true;                                              // sb_autoupdate
  bool find_routes_ = false;                                            // sb_findroutes
  bool colour_servers_ = false;                                         // sb_colorservers
  std::filesystem::path cache_dir_;
  bool list_cache_ = false, master_cache_ = true;  // sb_listcache, sb_mastercache
  bool cache_written_ = false;                     // this refresh's caches written
  bool told_done_ = true;                          // this refresh's f_sbrefreshdone run
  int32_t show_proxies_ = 1;                       // sb_showproxies
  std::string ignore_proxy_;                       // sb_ignore_proxy
  int32_t prox_per_s_ = 10, prox_retries_ = 3, prox_timeout_ = 1000;  // sb_prox*
  ge::console::Console* con_ = nullptr;  // the filter keys set the cvars through it
  ge::net::ServerFilter filter_;
  int hidden_ = 0;
  // Lists of servers on the web (`sb_masters` words `http://...`; `+http://...` the binary list
  // of 6-byte records, `+<n>http://...` of n-byte records), fetched at each refresh.
  struct WebList {
    std::string url;
    int record = 0;  // 0: text, an address a line
    ge::net::HttpFetch fetch;
    bool over = true;
  };
  std::vector<WebList> web_lists_;
  void web_frame();
};

}  // namespace shooters_edge::menu
