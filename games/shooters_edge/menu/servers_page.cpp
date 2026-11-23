// The server browser page: the list's config from the cvars, the keys, the table drawn
// (servers_page.hpp).
//
// - THE COLUMNS (`sb_show<column>` 0 or 1): the name always, then the address (`sb_showaddress`,
//   0), the mode, the game folder (`sb_showgamedir`, 0), the map (`sb_showmap`, 1), the players
//   (`sb_showplayers`, 1), the frag and time limits (`sb_showfraglimit`, `sb_showtimelimit`, 0),
//   the ping (`sb_showping`, 1) and the skill; the optional ones left out first when the name
//   would be squeezed. `sb_status` 0 drops the footer's two lines about the selected server.
// - THE SORT (`sb_sortservers`, "32"): column codes read left to right, the first that tells two
//   servers apart deciding: 1 name, 2 address, 3 ping, 4 game folder, 5 map, 6 players, 7 frag
//   limit, 8 time limit, and our own m mode and s skill; a `-` sorts the codes after it high to
//   low;
//   "" sorts the playing and not full first, then the full, then the empty. LEFT and RIGHT put a
//   column's code in front of the rest, the one already in front turned round.
//
// ge:summary The server browser page's refresh, keys, table, footer and empty states
// ge:keywords server-browser servers page table rows footer sort refresh join draw filter
// ge:skill ui net
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/servers_page.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

#include "console/console.hpp"
#include "ui/font.hpp"

namespace shooters_edge::menu {

namespace {

// Whether `w` is a web address (`http://` or `https://`, any case).
bool web_address(std::string_view w) {
  const auto starts = [&w](std::string_view p) {
    if (w.size() < p.size()) return false;
    for (size_t i = 0; i < p.size(); ++i)
      if (std::tolower(static_cast<unsigned char>(w[i])) != p[i]) return false;
    return true;
  };
  return starts("http://") || starts("https://");
}

}  // namespace

namespace {

namespace ui = ge::ui;
namespace gn = ge::net;

constexpr std::string_view kTitle = "SERVERS";
constexpr std::string_view kHelp =
    "ENTER JOIN   LEFT/RIGHT/D SORT   F/E/M/P FILTERS   R REFRESH   ESC BACK";
constexpr int32_t kPingBounds[] = {0, 50, 100, 150, 250};  // P steps through them
constexpr std::string_view kSep = "  \xc2\xb7  ";          // a middle dot

std::vector<std::string> split_words(const std::string& text) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < text.size()) {
    while (at < text.size() && (text[at] == ' ' || text[at] == ',' || text[at] == ';')) at++;
    size_t end = at;
    while (end < text.size() && text[end] != ' ' && text[end] != ',' && text[end] != ';') end++;
    if (end > at) out.push_back(text.substr(at, end - at));
    at = end;
  }
  return out;
}

// The columns LEFT and RIGHT put first, in order, with their codes in `sb_sortservers`.
struct SortCode {
  char code;
  gn::ServerColumn column;
  const char* key;  // ServerColumn::key's info key
  const char* word;
};
constexpr SortCode kSortCodes[] = {{'3', gn::ServerColumn::ping, "", "ping"},
                                   {'6', gn::ServerColumn::players, "", "players"},
                                   {'1', gn::ServerColumn::name, "", "name"},
                                   {'5', gn::ServerColumn::map, "", "map"},
                                   {'m', gn::ServerColumn::mode, "", "mode"},
                                   {'4', gn::ServerColumn::game, "", "game"},
                                   {'2', gn::ServerColumn::address, "", "address"},
                                   {'7', gn::ServerColumn::key, "fraglimit", "frag limit"},
                                   {'8', gn::ServerColumn::key, "timelimit", "time limit"},
                                   {'s', gn::ServerColumn::skill, "", "skill"}};

const SortCode* sort_code(char c) {
  for (const SortCode& k : kSortCodes)
    if (k.code == c) return &k;
  return nullptr;
}

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
  return s;
}

std::string fit(const ui::Font& font, std::string_view text, float size, float width) {
  const size_t n = font.fit(text, size, width);
  if (n >= text.size()) return std::string(text);
  const size_t m = font.fit(text, size, std::max(0.0f, width - font.advance("...", size)));
  return std::string(text.substr(0, m)) + "...";
}

// Text a server sent, made printable: the classic high-bit "gold" letters folded to plain ones,
// control bytes dropped.
std::string printable(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char ch : s) {
    auto c = static_cast<unsigned char>(ch & 0x7f);
    if (c >= 0x12 && c <= 0x1b) c = static_cast<unsigned char>('0' + (c - 0x12));
    if (c < 0x20 || c == 0x7f) continue;
    out += static_cast<char>(c);
  }
  return out;
}

}  // namespace

ServersPage::~ServersPage() {
  gn::close_transport(own_);
  gn::close_transport(route_own_);
}

void ServersPage::add_cvars(ge::console::Console& con) {
  con.cvars().bind({.name = "sb_lan",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "Whether JOIN A GAME looks for servers on the LAN, by a broadcast.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: no; 1: yes (the default)"},
                   &lan_);
  con.cvars().bind(
      {.name = "sb_masters",
       .fallback = "",
       .flags = ge::console::kArchive,
       .help = "The masters JOIN A GAME asks for their servers, beside those the loaded game's own "
               "server lists name.",
       .kind = ge::console::CvarKind::text,
       .values = "\"\": none of your own (the default); TEXT: host:port words with spaces between"},
      &masters_);
  con.cvars().bind(
      {.name = "sb_hubmasters",
       .fallback = "",
       .flags = ge::console::kArchive,
       .help = "The community masters JOIN A GAME asks: the ones our own servers announce "
               "themselves to (sv_master), a hub's.",
       .kind = ge::console::CvarKind::text,
       .values = "\"\": none (the default); TEXT: host:port words with spaces between (port "
                 "27900 when none is given)"},
      &hub_masters_);
  con.cvars().bind(
      {.name = "sb_servers",
       .fallback = "",
       .flags = ge::console::kArchive,
       .help = "The servers JOIN A GAME always asks, your favourites.",
       .kind = ge::console::CvarKind::text,
       .values = "\"\": none (the default); TEXT: host:port words with spaces between"},
      &servers_);
  con.cvars().bind({.name = "sb_hidefull",
                    .fallback = "0",
                    .flags = ge::console::kArchive,
                    .help = "Whether JOIN A GAME hides the servers with every slot taken (F).",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: shows them (the default); 1: hides them"},
                   &hide_full_);
  con.cvars().bind({.name = "sb_hideempty",
                    .fallback = "0",
                    .flags = ge::console::kArchive,
                    .help = "Whether JOIN A GAME hides the servers nobody is playing on (E).",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: shows them (the default); 1: hides them"},
                   &hide_empty_);
  con.cvars().bind(
      {.name = "sb_mode",
       .fallback = "",
       .flags = ge::console::kArchive,
       .help = "The one mode JOIN A GAME shows (M steps through those the servers name).",
       .kind = ge::console::CvarKind::text,
       .values = "\"\": every mode (the default); TEXT: a mode, such as ffa, team, duel or coop"},
      &mode_);
  con.cvars().bind({.name = "sb_maxping",
                    .fallback = "0",
                    .flags = ge::console::kArchive,
                    .help = "The highest ping JOIN A GAME shows, in milliseconds (P).",
                    .kind = ge::console::CvarKind::integer,
                    .min = 0,
                    .max = 999,
                    .values = "0: any ping (the default); 1-999: hide servers slower than this"},
                   &max_ping_);
  using ge::console::CvarKind;
  const auto flag = [&](const char* name, const char* fallback, const char* help,
                        const char* values, bool* field) {
    con.cvars().bind({.name = name,
                      .fallback = fallback,
                      .flags = ge::console::kArchive,
                      .help = help,
                      .kind = CvarKind::boolean,
                      .values = values},
                     field);
  };
  const auto number = [&](const char* name, const char* fallback, const char* help,
                          const char* values, int32_t lo, int32_t hi, int32_t* field) {
    con.cvars().bind({.name = name,
                      .fallback = fallback,
                      .flags = ge::console::kArchive,
                      .help = help,
                      .kind = CvarKind::integer,
                      .min = static_cast<double>(lo),
                      .max = static_cast<double>(hi),
                      .values = values},
                     field);
  };
  flag("sb_hidenotempty", "0", "Whether JOIN A GAME hides the servers somebody is on.",
       "0: shows them (the default); 1: only the empty servers are listed", &hide_not_empty_);
  flag("sb_hidedead", "1", "Whether JOIN A GAME hides the servers that didn't answer.",
       "0: lists them with no ping; 1: hides them (the default)", &hide_dead_);
  flag("sb_hidehighping", "0", "Whether JOIN A GAME hides the servers above sb_pinglimit.",
       "0: shows them (the default); 1: hides them", &hide_high_);
  number("sb_pinglimit", "80",
         "The ping above which sb_hidehighping hides a server, in milliseconds.",
         "80: the default; 0-999: that many milliseconds", 0, 999, &ping_limit_);
  con.cvars().bind(
      {.name = "sb_info_filter",
       .fallback = "",
       .flags = ge::console::kArchive,
       .help = "Words on the servers' own settings that show or hide them in JOIN A GAME.",
       .kind = CvarKind::text,
       .values =
           "\"\": every server (the default); TEXT: +key or -key words, each with =pattern if "
           "you like, such as +map=dm or -*gamedir=fortress"},
      &info_filter_);
  flag("sb_showaddress", "0", "Whether JOIN A GAME shows each server's address.",
       "0: no (the default); 1: yes", &show_address_);
  flag("sb_showping", "1", "Whether JOIN A GAME shows each server's ping.",
       "0: no; 1: yes (the default)", &show_ping_);
  flag("sb_showgamedir", "0", "Whether JOIN A GAME shows the game folder each server runs.",
       "0: no (the default); 1: yes", &show_game_);
  flag("sb_showmap", "1", "Whether JOIN A GAME shows each server's map.",
       "0: no; 1: yes (the default)", &show_map_);
  flag("sb_showplayers", "1", "Whether JOIN A GAME shows each server's players and slots.",
       "0: no; 1: yes (the default)", &show_players_);
  flag("sb_showfraglimit", "0", "Whether JOIN A GAME shows each server's frag limit.",
       "0: no (the default); 1: yes", &show_fl_);
  flag("sb_showtimelimit", "0", "Whether JOIN A GAME shows each server's time limit.",
       "0: no (the default); 1: yes", &show_tl_);
  flag("sb_status", "1", "Whether JOIN A GAME shows the selected server's facts and players.",
       "0: no; 1: yes (the default)", &status_);
  con.cvars().bind(
      {.name = "sb_sortservers",
       .fallback = "32",
       .flags = ge::console::kArchive,
       .help = "How JOIN A GAME sorts its servers: column codes, the first that differs deciding.",
       .kind = CvarKind::text,
       .values =
           "\"\": the busy servers first; TEXT: codes 1 name, 2 address, 3 ping, 4 game, 5 map, "
           "6 players, 7 frag limit, 8 time limit, m mode, s skill, a - before the ones to "
           "sort high to low (32, the default: by ping, then address)"},
      &sort_servers_);
  number("sb_inforetries", "3", "How many times JOIN A GAME asks a server before giving it up.",
         "3: the default; 1-10: that many queries", 1, 10, &info_retries_);
  number("sb_infotimeout", "1000", "How long JOIN A GAME waits for a server before asking again.",
         "1000: the default; 100-10000: that many milliseconds", 100, 10000, &info_timeout_);
  number("sb_infospersec", "100", "How many servers JOIN A GAME asks a second.",
         "100: the default; 10-1000: that many queries a second", 10, 1000, &infos_per_s_);
  number("sb_masterretries", "3", "How many times JOIN A GAME asks a master for its servers.",
         "3: the default; 1-10: that many asks", 1, 10, &master_retries_);
  number("sb_mastertimeout", "1000", "How long JOIN A GAME waits for a master before asking again.",
         "1000: the default; 100-10000: that many milliseconds", 100, 10000, &master_timeout_);
  con.cvars().bind(
      {.name = "sb_liveupdate",
       .fallback = "2",
       .flags = ge::console::kArchive,
       .help = "How often JOIN A GAME asks the selected server again, so its players "
               "and scores stay current.",
       .kind = CvarKind::number,
       .min = 0,
       .max = 60,
       .values = "0: never; 2: every 2 seconds (the default); 1-60: that many seconds"},
      &live_update_);
  number("sb_autohide", "1", "Whether joining a server from JOIN A GAME closes the menu.",
         "0: the menu stays open; 1: it closes (the default); 2: it closes too", 0, 2, &autohide_);
  flag("sb_colorservers", "0",
       "Whether JOIN A GAME colours each server: grey when locked, green when very near.",
       "0: no (the default); 1: grey for a password or a cheat guard, green under 30 ms",
       &colour_servers_);
  flag("sb_findroutes", "0",
       "Whether JOIN A GAME looks for faster routes to the servers through forwarders.",
       "0: no (the default); 1: yes, and joining goes the fastest way", &find_routes_);
  number("sb_showproxies", "1", "Whether JOIN A GAME lists the forwarders among the servers.",
         "0: hidden; 1: listed (the default); 2: only the forwarders", 0, 2, &show_proxies_);
  con.cvars().bind({.name = "sb_ignore_proxy",
                    .fallback = "",
                    .flags = ge::console::kArchive,
                    .help = "Forwarders JOIN A GAME leaves out of its routes.",
                    .kind = CvarKind::text,
                    .values = "\"\": none (the default); TEXT: addresses, or parts of them, with "
                              "spaces between"},
                   &ignore_proxy_);
  number("sb_proxinfopersec", "10", "How many forwarders JOIN A GAME asks a second.",
         "10: the default; 1-100: that many a second", 1, 100, &prox_per_s_);
  number("sb_proxretries", "3", "How many times JOIN A GAME asks a forwarder for its servers.",
         "3: the default; 1-10: that many asks", 1, 10, &prox_retries_);
  number("sb_proxtimeout", "1000",
         "How long JOIN A GAME waits for a forwarder before asking again.",
         "1000: the default; 100-10000: that many milliseconds", 100, 10000, &prox_timeout_);
  con.add_command("connectbr",
                  "connectbr <host>[:port]: join a server by the fastest route JOIN A GAME found",
                  [this](ge::console::Console& c, const ge::console::Args& a) {
                    if (a.size() < 2)
                      c.print("connectbr: name a server (connectbr 192.168.1.20)");
                    else
                      connect_by_route(c, a[1], false);
                  });
  con.add_command("observebr",
                  "observebr <host>[:port]: watch a server by the fastest route JOIN A GAME found",
                  [this](ge::console::Console& c, const ge::console::Args& a) {
                    if (a.size() < 2)
                      c.print("observebr: name a server (observebr 192.168.1.20)");
                    else
                      connect_by_route(c, a[1], true);
                  });
  flag("sb_listcache", "0",
       "Whether JOIN A GAME keeps its list between sessions, shown at once the next time.",
       "0: no (the default); 1: yes, in sb/servers.txt in the game's folder", &list_cache_);
  flag("sb_mastercache", "1",
       "Whether each master's last list is kept, asked when the master is silent.",
       "0: no; 1: yes (the default), in sb/cache in the game's folder", &master_cache_);
  flag("sb_autoupdate", "1", "Whether JOIN A GAME looks for servers again each time it opens.",
       "0: the last list stays until you press R; 1: yes (the default)", &autoupdate_);
  con_ = &con;
}

namespace {

// Quake II's `ui_sortservers` as keys: one column either way, ties by the name the same way.
std::vector<gn::ServerSortKey> game_sort_keys(int32_t v) {
  if (v == 0) return {};  // the order found: `update` puts the rows back in the list's order
  constexpr gn::ServerColumn kColumn[] = {gn::ServerColumn::name, gn::ServerColumn::game,
                                          gn::ServerColumn::map, gn::ServerColumn::players,
                                          gn::ServerColumn::ping};
  const bool back = v < 0;
  const gn::ServerColumn c = kColumn[std::clamp(std::abs(v), 1, 5) - 1];
  std::vector<gn::ServerSortKey> keys = {
      {c, c == gn::ServerColumn::players ? !back : back, {}}};  // its players run most first
  if (c != gn::ServerColumn::name) keys.push_back({gn::ServerColumn::name, back, {}});
  return keys;
}

}  // namespace

// The servers asked a second: `sb_infospersec`, or Quake II's `ui_pingrate` while it is bound.
int32_t ServersPage::ping_rate() const {
  if (game_ping_rate < 0) return infos_per_s_;
  if (game_ping_rate > 0) return std::min(game_ping_rate, 100);
  const ge::console::Cvar* rate = con_ != nullptr ? con_->cvars().find("rate") : nullptr;
  const int32_t bytes = rate != nullptr ? std::atoi(rate->value.c_str()) : 15000;
  return std::clamp(bytes / 450, 1, 100);  // a reply's 450 bytes in the player's rate
}

void ServersPage::read_settings() {
  if (game_sort != game_sort_seen_) {
    game_sort_seen_ = game_sort;
    sort_seen_ = "\x01";  // read again below
  }
  if (sort_servers_ != sort_seen_) {
    sort_seen_ = sort_servers_;
    sort_keys_.clear();
    if (game_sort != kNoGameSort) sort_keys_ = game_sort_keys(game_sort);
    bool down = false;
    for (const char c : game_sort != kNoGameSort ? std::string() : sort_servers_) {
      if (c == '-') {
        down = true;
        continue;
      }
      const SortCode* k = sort_code(c);
      if (k == nullptr) break;  // an unknown code ends the sort
      sort_keys_.push_back({k->column, down, k->key});
    }
  }
  if (info_filter_ != info_seen_) {
    info_seen_ = info_filter_;
    std::vector<std::string> problems;
    info_rules_ = gn::parse_info_filter(info_filter_, &problems);
    info_problem_ = problems.empty() ? "" : problems.front();
  }
}

bool ServersPage::sorted_by(gn::ServerColumn c) const {
  return !sort_keys_.empty() && sort_keys_.front().column == c;
}

std::string ServersPage::sort_words() const {
  if (game_sort == 0) return "as found";  // Quake II's ui_sortservers 0
  if (sort_keys_.empty()) return "busy first";
  std::string out;
  for (size_t i = 0; i < sort_keys_.size() && i < 2; i++) {
    const gn::ServerSortKey& k = sort_keys_[i];
    const char* word = gn::server_column_name(k.column);
    for (const SortCode& c : kSortCodes)
      if (c.column == k.column && k.key == c.key) word = c.word;
    if (i > 0) out += ", then ";
    out += word;
    if (k.descending) out += " (high to low)";
  }
  return out;
}

void ServersPage::sort_step(int dir) {
  if (game_sort != kNoGameSort) {  // Quake II's columns in turn, its setting left as it is
    const int32_t at = std::abs(game_sort);
    game_sort = ((at == 0 ? (dir > 0 ? 0 : 1) : at) - 1 + dir + 5) % 5 + 1;
    return;
  }
  // The code in front now, and the next or last in LEFT and RIGHT's order.
  const std::string& text = sort_servers_;
  const size_t lead = !text.empty() && text[0] == '-' ? 1 : 0;
  int at = -1;
  constexpr int kN = static_cast<int>(std::size(kSortCodes));
  for (int i = 0; i < kN; i++)
    if (lead < text.size() && kSortCodes[i].code == text[lead]) at = i;
  const char next = kSortCodes[((at < 0 ? (dir > 0 ? -1 : 0) : at) + dir + kN) % kN].code;
  std::string rest;
  for (const char c : text)
    if (c != next && c != '-') rest += c;
  std::string out(1, next);
  out += rest;
  if (out.size() > 31) out.resize(31);
  set_cvar("sb_sortservers", out);
}

void ServersPage::set_cvar(std::string_view name, const std::string& value) {
  if (con_ != nullptr) {
    con_->cvars().set(name, value, ge::console::Source::local);
    return;
  }
  if (name == "sb_hidefull") hide_full_ = value == "1";
  if (name == "sb_hideempty") hide_empty_ = value == "1";
  if (name == "sb_mode") mode_ = value;
  if (name == "sb_maxping") max_ping_ = std::atoi(value.c_str());
  if (name == "sb_sortservers") sort_servers_ = value;
}

bool ServersPage::filter_key(char letter) {
  if (letter == 'f') {
    set_cvar("sb_hidefull", hide_full_ ? "0" : "1");
  } else if (letter == 'e') {
    set_cvar("sb_hideempty", hide_empty_ ? "0" : "1");
  } else if (letter == 'm') {
    // The next of the modes the answers name, A to Z, then every mode again.
    std::vector<std::string> modes;
    for (const gn::ServerEntry& e : list_.entries) {
      if (!e.answered || e.info.mode.empty()) continue;
      std::string m = printable(e.info.mode);
      for (char& c : m) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
      if (std::find(modes.begin(), modes.end(), m) == modes.end()) modes.push_back(m);
    }
    std::sort(modes.begin(), modes.end());
    const auto it = std::upper_bound(modes.begin(), modes.end(), mode_);
    set_cvar("sb_mode", mode_.empty()       ? (modes.empty() ? "" : modes.front())
                        : it == modes.end() ? ""
                                            : *it);
  } else if (letter == 'd' && game_sort != kNoGameSort) {
    game_sort = -game_sort;  // Quake II's sort turned round, its setting left as it is
  } else if (letter == 'd') {
    // The sort turned round: a - in front, or the one there taken away.
    std::string t = sort_servers_;
    if (!t.empty() && t[0] == '-')
      t.erase(0, 1);
    else
      t.insert(0, "-");
    set_cvar("sb_sortservers", t);
  } else if (letter == 'p') {
    int32_t next = kPingBounds[0];
    for (const int32_t b : kPingBounds)
      if (b > max_ping_) {
        next = b;
        break;
      }
    set_cvar("sb_maxping", std::to_string(next));
  } else {
    return false;
  }
  return true;
}

int ServersPage::effective_ping() const {
  int bound = max_ping_;
  if (hide_high_ && (bound <= 0 || ping_limit_ < bound)) bound = std::max(1, ping_limit_);
  return bound;
}

std::string ServersPage::filter_words() const {
  std::string out;
  const auto add = [&](const std::string& w) { out += (out.empty() ? "" : ", ") + w; };
  if (hide_full_ && hide_empty_)
    add("NOT FULL OR EMPTY");
  else if (hide_full_)
    add("NOT FULL");
  else if (hide_empty_)
    add("NOT EMPTY");
  if (hide_not_empty_) add("EMPTY ONLY");
  if (!hide_dead_) add("SILENT SHOWN");
  if (!mode_.empty()) add(upper(mode_) + " ONLY");
  const int bound = effective_ping();
  if (bound > 0) add("PING UNDER " + std::to_string(bound));
  if (!info_rules_.empty()) add("INFO FILTER");
  return out;
}

bool ServersPage::open(std::string* why) {
  note_.clear();
  if (!has_query()) {
    note_ = "this game names no server protocol";
    if (why) *why = note_;
    return false;
  }
  gn::Transport* t = borrowed_;
  if (t == nullptr) {
    if (own_.kind == gn::TransportKind::none) {
      std::string error;
      if (!gn::open_udp_transport(own_, gn::ipv4(0, 0, 0, 0, 0), &error)) {
        note_ = "the browser's socket did not open: " + error +
                ". Check that the computer is on a network.";
        if (why) *why = note_;
        return false;
      }
    }
    t = &own_;
  }
  config_ = gn::ServerListConfig{};
  // Names resolved once here, when the player opens the page; a name that does not resolve is
  // left out and said in the footer.
  std::vector<std::string> masters = split_words(masters_);
  masters.insert(masters.end(), install_masters_.begin(), install_masters_.end());
  std::vector<std::string> servers = split_words(servers_);
  bool lan = lan_, hub = true, book = true;
  const bool sourced = !sources_.empty();
  if (sourced) {  // one open's sources in place of the configured ones
    masters.clear();
    servers.clear();
    lan = hub = book = false;
    for (const std::string& w : std::exchange(sources_, {})) {
      const std::string low = ge::console::lower(w);
      if (low == "favorites://") {
        book = true;
      } else if (low == "broadcast://") {
        lan = true;
      } else if (low.rfind("quake2://", 0) == 0) {
        servers.push_back(w.substr(9));
      } else if (low.rfind("file:///", 0) == 0) {
        // A list of addresses in the game's files, one a line; none there lists nothing.
        if (const std::optional<std::string> text =
                read_file_ ? read_file_(w.substr(8)) : std::nullopt) {
          std::string words = *text;
          std::replace_if(
              words.begin(), words.end(),
              [](char c) { return c == '\n' || c == '\r' || c == '\t'; }, ' ');
          for (const std::string& line : split_words(words)) servers.push_back(line);
        }
      } else if (const size_t at = w.find_first_not_of("+-0123456789");
                 at != std::string::npos && web_address(std::string_view(w).substr(at))) {
        masters.push_back(w);
      } else if (con_ != nullptr) {
        con_->print("Ignoring invalid master URL: " + w);
      }
    }
  }
  if (lan) {
    for (uint32_t p = lan_first_; p <= lan_last_; p++)
      config_.lan.push_back(borrowed_ != nullptr
                                ? gn::sim_broadcast(static_cast<uint16_t>(p))
                                : gn::ipv4(255, 255, 255, 255, static_cast<uint16_t>(p)));
  }
  auto resolve = [&](const std::string& text, uint16_t port, std::vector<gn::Address>& into) {
    gn::Address a;
    if (gn::parse_address(text, a)) {
      // already an address (an IPv4 or a simulated node)
    } else if (std::string error; !gn::resolve_ipv4(text, port, a, &error)) {
      if (note_.empty()) note_ = error;
      return;
    }
    if (std::find(into.begin(), into.end(), a) == into.end()) into.push_back(a);
  };
  web_lists_.clear();
  for (const std::string& m : masters) {
    // A web list: `http://...`, `+http://...` (6-byte records) or `+<n>http://...`.
    std::string_view w = m;
    int record = 0;
    if (!w.empty() && (w[0] == '+' || w[0] == '-')) {
      w.remove_prefix(1);
      int n = 0;
      bool digits = false;
      while (!w.empty() && w[0] >= '0' && w[0] <= '9' && n < 100000) {
        n = n * 10 + (w[0] - '0');
        digits = true;
        w.remove_prefix(1);
      }
      record = digits ? n : 6;
      if (record < 6 || !web_address(w)) {
        if (con_ != nullptr) con_->print("Ignoring invalid master URL: " + m);
        continue;
      }
    }
    if (web_address(w)) {
      web_lists_.push_back({.url = std::string(w), .record = record, .fetch = {}, .over = true});
      continue;
    }
    resolve(m, 27000, config_.masters);
  }
  if (query_.protocol != 0 && hub)
    for (const std::string& m : split_words(hub_masters_))
      resolve(m, gn::kMasterPort, config_.own_masters);
  for (const std::string& s : servers) resolve(s, lan_first_, config_.servers);
  // The address book a game keeps (Quake II's `adr0`, `adr1`, ...): read in order until one is
  // not registered, the empty ones skipped.
  for (int k = 0; con_ != nullptr && book; k++) {
    const ge::console::Cvar* adr = con_->cvars().find("adr" + std::to_string(k));
    if (adr == nullptr) break;
    for (const std::string& s : split_words(adr->value)) resolve(s, lan_first_, config_.servers);
  }
  config_.tries = info_retries_;
  config_.timeout_us = int64_t{info_timeout_} * 1000;
  config_.gap_us = 1000000 / std::max(1, ping_rate());
  config_.master_tries = master_retries_;
  config_.master_timeout_us = int64_t{master_timeout_} * 1000;
  const bool first = !inited_ || list_.transport != t;
  if (first) gn::server_list_init(list_, config_, query_, *t);
  list_.config = config_;
  inited_ = true;
  read_settings();
  // sb_listcache: the first open of a session shows the kept list, asked again only when
  // sb_autoupdate says so.
  const bool from_cache = first && list_cache_ && read_list_cache();
  if ((first && !from_cache) || autoupdate_ || sourced)
    refresh();
  else
    build_rows();
  return true;
}

std::filesystem::path ServersPage::master_cache_path(const gn::Address& master) const {
  std::string name = gn::to_string(master);
  for (char& c : name)
    if (c == '.' || c == ':') c = '_';
  return cache_dir_ / "sb" / "cache" / (name + ".txt");
}

namespace {

bool read_text(const std::filesystem::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::ostringstream all;
  all << in.rdbuf();
  out = all.str();
  return true;
}

bool write_text(const std::filesystem::path& path, const std::string& text) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
  return static_cast<bool>(out);
}

}  // namespace

bool ServersPage::read_list_cache() {
  if (cache_dir_.empty()) return false;
  const std::filesystem::path path = cache_dir_ / "sb" / "servers.txt";
  std::string text;
  if (!read_text(path, text)) return false;
  int bad = 0;
  const int n = gn::server_list_read_cache(list_, text, &bad);
  if (con_ != nullptr)
    con_->print(n > 0     ? "Successfully read " + std::to_string(n) + " servers"
                : bad > 0 ? "Corrupted data"
                          : "No servers read");
  if (n <= 0) return false;
  cache_written_ = true;  // the list is the file's: nothing new to write back
  return true;
}

void ServersPage::add_cached() {
  // A master's list kept from its last answer is asked beside it, so a silent master still
  // lists its servers (read whatever sb_mastercache is, as only its writing is switched).
  if (cache_dir_.empty()) return;
  for (const gn::Address& m : list_.config.masters) {
    std::string text;
    if (!read_text(master_cache_path(m), text)) continue;
    for (const gn::Address& a : gn::parse_address_lines(text))
      gn::server_list_add(list_, a, gn::kServerFromCache, now_us_);
  }
}

void ServersPage::write_caches() {
  if (cache_written_ || cache_dir_.empty() || !gn::server_list_done(list_, now_us_)) return;
  cache_written_ = true;
  if (master_cache_)
    for (size_t m = 0; m < list_.masters.size() && m < list_.config.masters.size(); m++)
      if (!list_.masters[m].got.empty())
        write_text(master_cache_path(list_.config.masters[m]),
                   gn::address_lines(list_.masters[m].got));
  if (!list_cache_) return;
  const std::filesystem::path path = cache_dir_ / "sb" / "servers.txt";
  const bool ok = write_text(path, gn::server_list_cache_text(list_));
  if (con_ != nullptr)
    con_->print(ok ? std::string("Wrote server list contents to disk")
                   : "Couldn't write " + path.string() + ".");
}

void ServersPage::refresh() {
  gn::server_list_refresh(list_, now_us_);
  // The web lists asked again: no redirect followed, given up after `cl_http_blocking_timeout` s
  // of silence (15 when the game has none).
  double wait_s = 15;
  if (con_ != nullptr)
    if (const ge::console::Cvar* v = con_->cvars().find("cl_http_blocking_timeout"))
      wait_s = std::max(1.0, std::atof(v->value.c_str()));
  for (WebList& l : web_lists_) {
    gn::HttpOptions o;
    o.user_agent = "shooters_edge";
    o.redirects = 0;
    o.stall_s = wait_s;
    o.most_bytes = 1024 * 1024 - 1;
    gn::http_start(l.fetch, l.url, o, static_cast<double>(now_us_) / 1e6);
    l.over = false;
  }
  add_cached();
  cache_written_ = false;
  routing_ = routes_ready_ = false;
  table_ = {};
  selected_ = 0;
  first_ = 0;
  build_rows();
}

void ServersPage::frame(double dt) {
  carry_us_ += dt * 1e6;
  const auto whole = static_cast<int64_t>(carry_us_);
  carry_us_ -= static_cast<double>(whole);
  now_us_ += whole;
  if (!inited_) return;
  read_settings();
  // The selected server asked again every `sb_liveupdate` s once the refresh is done.
  live_s_ += dt;
  if (live_update_ >= 1 && live_s_ >= live_update_ && gn::server_list_done(list_, now_us_) &&
      selected_ < static_cast<int>(rows_.size())) {
    gn::server_list_requery(list_, rows_[static_cast<size_t>(selected_)]);
    live_s_ = 0;
  }
  web_frame();
  gn::server_list_update(list_, now_us_);
  write_caches();
  route_frame();
  // The selection stays on its server as the rows sort themselves around it.
  const int was = selected_ >= 0 && selected_ < static_cast<int>(rows_.size())
                      ? rows_[static_cast<size_t>(selected_)]
                      : -1;
  build_rows();
  if (was >= 0) {
    const auto it = std::find(rows_.begin(), rows_.end(), was);
    if (it != rows_.end()) selected_ = static_cast<int>(it - rows_.begin());
  }
}

// The web lists moved on; each that came read into the list, each that failed said.
void ServersPage::web_frame() {
  for (WebList& l : web_lists_) {
    if (l.over || !gn::http_step(l.fetch, static_cast<double>(now_us_) / 1e6)) continue;
    l.over = true;
    const gn::HttpFetch& f = l.fetch;
    if (f.state != gn::HttpState::done || f.status >= 400) {
      if (con_ != nullptr)
        con_->print("[HTTP] Failed to fetch '" + l.url + "': " +
                    (f.state == gn::HttpState::done ? gn::http_status_text(f.status) : f.error));
      continue;
    }
    std::vector<gn::Address> found;
    if (l.record == 0) {
      found = gn::parse_address_lines(
          std::string_view(reinterpret_cast<const char*>(f.body.data()), f.body.size()));
    } else {
      const auto n = static_cast<size_t>(l.record);
      for (size_t at = 0; at + n <= f.body.size(); at += n) {
        const uint8_t* b = f.body.data() + at;
        found.push_back(gn::ipv4(b[0], b[1], b[2], b[3], static_cast<uint16_t>(b[4] << 8 | b[5])));
      }
    }
    for (const gn::Address& a : found)
      gn::server_list_add(list_, a, gn::kServerFromMaster, now_us_);
  }
}

void ServersPage::build_rows() {
  read_settings();
  filter_ = gn::ServerFilter{};
  filter_.hide_full = hide_full_;
  filter_.hide_empty = hide_empty_;
  filter_.hide_not_empty = hide_not_empty_;
  filter_.hide_silent = hide_dead_;
  filter_.mode = mode_;
  filter_.max_ping_ms = effective_ping();
  filter_.info = info_rules_;
  gn::server_list_view(list_, filter_, sort_keys_, rows_);
  if (game_sort == 0)  // Quake II's 0: the order the servers were found, the silent last
    std::stable_sort(rows_.begin(), rows_.end(), [&](int a, int b) {
      const bool sa = list_.entries[static_cast<size_t>(a)].ping_ms < 0;
      const bool sb = list_.entries[static_cast<size_t>(b)].ping_ms < 0;
      return sa != sb ? sb : a < b;
    });
  if (show_proxies_ != 1)  // sb_showproxies: the forwarders hidden (0) or alone (2)
    rows_.erase(std::remove_if(rows_.begin(), rows_.end(),
                               [&](int i) {
                                 return proxy_row(list_.entries[static_cast<size_t>(i)].info) !=
                                        (show_proxies_ == 2);
                               }),
                rows_.end());
  // The answered servers the filters leave out, for the title row.
  std::vector<int> all;
  gn::ServerFilter none;
  none.hide_silent = hide_dead_;
  gn::server_list_view(list_, none, gn::ServerSort::ping, all);
  hidden_ = std::max(0, static_cast<int>(all.size() - rows_.size()));
  if (selected_ >= static_cast<int>(rows_.size()))
    selected_ = std::max(0, static_cast<int>(rows_.size()) - 1);
}

int ServersPage::hit(float x, float y) const {
  for (size_t i = 0; i < cells_.size(); i++)
    if (cells_[i].contains(x, y)) return first_ + static_cast<int>(i);
  return -1;
}

ServersAction ServersPage::input(const ui::MenuInput& in, bool refresh_key) {
  if (in.back) return ServersAction::leave;
  if (refresh_key && inited_) {
    refresh();
    return ServersAction::none;
  }
  if (in.letter != 0 && filter_key(in.letter)) {
    const int was =
        selected_ < static_cast<int>(rows_.size()) ? rows_[static_cast<size_t>(selected_)] : -1;
    build_rows();
    const auto it = std::find(rows_.begin(), rows_.end(), was);
    selected_ = it != rows_.end() ? static_cast<int>(it - rows_.begin()) : 0;
    return ServersAction::none;
  }
  const int n = static_cast<int>(rows_.size());
  if (in.up && selected_ > 0) selected_--;
  if (in.down && selected_ + 1 < n) selected_++;
  if (in.left || in.right) {
    sort_step(in.right ? 1 : -1);
    read_settings();
    const int was = selected_ < n ? rows_[static_cast<size_t>(selected_)] : -1;
    build_rows();
    const auto it = std::find(rows_.begin(), rows_.end(), was);
    selected_ = it != rows_.end() ? static_cast<int>(it - rows_.begin()) : 0;
  }
  bool join = in.accept;
  if (in.click || in.pointer_moved) {
    const int at = hit(in.pointer_x, in.pointer_y);
    if (at >= 0 && at < n) {
      selected_ = at;
      join = join || in.click;
    } else if (in.click) {
      join = false;
    }
  }
  if (join && selected_ < n) {
    chosen_ = gn::to_string(
        list_.entries[static_cast<size_t>(rows_[static_cast<size_t>(selected_)])].address);
    return ServersAction::join;
  }
  return ServersAction::none;
}

void ServersPage::draw(ui::DrawList& list, int width, int height, const ui::MenuStyle& style) {
  problems_.clear();
  cells_.clear();
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const float s = std::min(w, h) / 720.0f;
  const bool portrait = h > w;
  const ui::Font& bold = style.title_font ? *style.title_font : ui::ui_font(ui::Weight::semibold);
  const ui::Font& small = style.small_font ? *style.small_font : ui::ui_font(ui::Weight::medium);
  const float margin = (portrait ? 24.0f : 48.0f) * s;
  const float ts = std::max(s, 0.75f);
  const float title = style.title_size * ts, word = style.small_size * ts;
  const float line = word * 1.6f;
  const float inner = w - 2 * margin;
  list.rect({0, 0, w, h}, ui::hex(0x000000, 0.66f));

  // The title row: the page's name, then how the refresh stands.
  const float title_base = margin + title;
  list.text(bold, title, margin, title_base, kTitle, style.text, ui::Align::left, style.tracking);
  const int answered = gn::server_list_answered(list_);
  const int known = static_cast<int>(list_.entries.size());
  const bool going = inited_ && !gn::server_list_done(list_, now_us_);
  std::string where = std::to_string(answered) + (answered == 1 ? " SERVER" : " SERVERS");
  if (known > answered) where += " OF " + std::to_string(known) + " ASKED";
  where += std::string(kSep) + (going ? "REFRESHING" : "SORTED BY " + upper(sort_words()));
  if (const std::string f = filter_words(); !f.empty())
    where += std::string(kSep) + f + " (" + std::to_string(hidden_) + " HIDDEN)";
  const float title_w = bold.advance(kTitle, title, style.tracking);
  if (title_w + small.advance(where, word) + 24 * s > inner)
    list.text(small, word, margin, title_base + line, fit(small, where, word, inner),
              style.text_dim);
  else
    list.text(small, word, w - margin, title_base, where, style.text_dim, ui::Align::right);

  // The columns: the name takes what the others leave; the optional ones go first when it would
  // be squeezed.
  struct Col {
    std::string_view head;
    float width;  // at the reference, px; 0: the name's
    bool right;
    gn::ServerColumn column;
    const char* key;
  };
  const bool narrow = inner < 760 * s;
  Col cols[] = {{"NAME", 0, false, gn::ServerColumn::name, ""},
                {"ADDRESS", show_address_ ? 176.0f : 0, false, gn::ServerColumn::address, ""},
                {"MODE", narrow ? 0.0f : 96, false, gn::ServerColumn::mode, ""},
                {"GAME", show_game_ ? 96.0f : 0, false, gn::ServerColumn::game, ""},
                {"MAP", show_map_ ? 120.0f : 0, false, gn::ServerColumn::map, ""},
                {"PLAYERS", show_players_ ? 104.0f : 0, true, gn::ServerColumn::players, ""},
                {"FL", show_fl_ ? 48.0f : 0, true, gn::ServerColumn::key, "fraglimit"},
                {"TL", show_tl_ ? 48.0f : 0, true, gn::ServerColumn::key, "timelimit"},
                {"PING", show_ping_ ? 72.0f : 0, true, gn::ServerColumn::ping, ""},
                {"SKILL", narrow ? 0.0f : 64, true, gn::ServerColumn::skill, ""}};
  constexpr size_t kCols = std::size(cols);
  const auto fixed_width = [&] {
    float f = 0;
    for (size_t c = 1; c < kCols; c++) f += cols[c].width * s;
    return f;
  };
  for (const size_t drop : {size_t{1}, size_t{3}, size_t{6}, size_t{7}, size_t{2}, size_t{9}})
    if (inner - 24 * s - fixed_width() < 200 * s) cols[drop].width = 0;
  const float fixed = fixed_width();
  float xs[kCols], ws[kCols];
  float x = margin + 12 * s;
  for (size_t c = 0; c < kCols; c++) {
    ws[c] = c == 0 ? std::max(80 * s, inner - 24 * s - fixed) : cols[c].width * s;
    xs[c] = x;
    x += ws[c];
  }
  const float head_base = title_base + (portrait ? 2.4f : 1.6f) * line;
  for (size_t c = 0; c < kCols; c++) {
    if (ws[c] <= 0) continue;
    const bool sorted = sorted_by(cols[c].column) && (cols[c].column != gn::ServerColumn::key ||
                                                      sort_keys_.front().key == cols[c].key);
    const float tx = cols[c].right ? xs[c] + ws[c] - 8 * s : xs[c];
    list.text(bold, word * 0.85f, tx, head_base, cols[c].head,
              sorted ? style.accent_hi : style.text_dim,
              cols[c].right ? ui::Align::right : ui::Align::left, style.tracking);
  }

  // The footer: the selected server's facts and players, over the keys.
  const float help_base = h - margin;
  const float roster_base = help_base - line * 1.3f;
  const float facts_base = roster_base - line;
  list.text(small, word, margin, help_base, fit(small, kHelp, word, inner), style.text_dim);
  const gn::ServerEntry* e =
      selected_ < static_cast<int>(rows_.size())
          ? &list_.entries[static_cast<size_t>(rows_[static_cast<size_t>(selected_)])]
          : nullptr;
  const std::string& note = !note_.empty() ? note_ : info_problem_;
  if (!note.empty()) {
    list.text(small, word, margin, facts_base, fit(small, note, word, inner), style.accent_hi);
  } else if (e != nullptr && status_) {
    std::string facts = gn::to_string(e->address);
    if (!e->info.game.empty()) facts += std::string(kSep) + upper(e->info.game);
    if (!e->info.state.empty()) facts += std::string(kSep) + upper(printable(e->info.state));
    for (const std::string& m : e->info.mods) facts += std::string(kSep) + upper(printable(m));
    if (e->info.max_spectators > 0 || e->info.spectators > 0)
      facts += std::string(kSep) + std::to_string(e->info.spectators) + "/" +
               std::to_string(e->info.max_spectators) + " WATCHING";
    if (e->from & gn::kServerFromLan) facts += std::string(kSep) + "LAN";
    if (const std::string r = route_words(rows_[static_cast<size_t>(selected_)]); !r.empty())
      facts += std::string(kSep) + r;
    list.text(small, word, margin, facts_base, fit(small, facts, word, inner), style.text_dim);
  }
  if (e != nullptr && status_) {
    std::string who;
    for (const gn::ServerPlayer& p : e->info.roster) {
      if (p.spectator) continue;
      if (!who.empty()) who += ", ";
      who += printable(p.name);
      if (!p.team.empty()) who += " (" + printable(p.team) + ")";
      who += " " + std::to_string(p.frags);
    }
    if (who.empty()) who = "NOBODY PLAYING";
    list.text(small, word, margin, roster_base, fit(small, who, word, inner), style.text);
  }

  // The rows.
  const float top = head_base + line * 0.6f;
  const float bottom = (status_ || !note.empty() ? facts_base : help_base) - line * 1.2f;
  const float row_h = std::max(word * 1.9f, 34 * s);
  const int fits = std::max(1, static_cast<int>((bottom - top) / row_h));
  const int n = static_cast<int>(rows_.size());
  if (selected_ < first_) first_ = selected_;
  if (selected_ >= first_ + fits) first_ = selected_ - fits + 1;
  first_ = std::clamp(first_, 0, std::max(0, n - fits));
  if (n == 0) {
    const float cy = (top + bottom) * 0.5f;
    const bool filtered = hidden_ > 0;
    const std::string_view big = filtered ? "EVERY SERVER FILTERED OUT"
                                 : going  ? "LOOKING FOR SERVERS"
                                          : "NO SERVERS FOUND";
    list.text(bold, title * 0.7f, w * 0.5f, cy, big, style.text, ui::Align::center, style.tracking);
    const std::string how =
        filtered
            ? std::to_string(hidden_) + (hidden_ == 1 ? " server answered" : " servers answered") +
                  " and the filters hide them. F, E, M and P change the filters."
        : going ? "On the LAN and at the masters."
        : config_.masters.empty() && config_.own_masters.empty()
            ? "None answered on the LAN. Name masters with sb_masters, or servers with sb_servers."
            : "None answered on the LAN or at the masters. R asks again.";
    list.text(small, word, w * 0.5f, cy + line * 1.4f, fit(small, how, word, inner), style.text_dim,
              ui::Align::center);
    return;
  }
  for (int r = first_; r < n && r < first_ + fits; r++) {
    const gn::ServerEntry& se = list_.entries[static_cast<size_t>(rows_[static_cast<size_t>(r)])];
    const gn::ServerInfo& in = se.info;
    const ui::Rect cell{margin, top + static_cast<float>(r - first_) * row_h, inner, row_h - 4 * s};
    cells_.push_back(cell);
    const bool sel = r == selected_;
    list.rect(cell, sel ? style.accent : style.slab, style.radius * s);
    const float base = cell.y + cell.h * 0.5f + word * 0.36f;
    const bool full = in.max_players > 0 && in.players >= in.max_players;
    ui::Color tc = sel ? style.text_focus : style.text;
    ui::Color dim = sel ? style.text_focus : style.text_dim;
    if (ui::Color c; colour_servers_ && row_colour(in, se.ping_ms, c)) tc = dim = c;
    const std::string name = (in.locked ? "* " : "") + printable(in.name);
    const auto key = [&](const char* k) {
      for (const auto& [kk, v] : in.keys)
        if (kk == k) return printable(v);
      return std::string("-");
    };
    const std::string cells[kCols] = {
        name,
        gn::to_string(se.address),
        upper(printable(in.mode)),
        printable(in.game),
        printable(in.map),
        std::to_string(in.players) + " / " + std::to_string(in.max_players),
        key("fraglimit"),
        key("timelimit"),
        ping_words(rows_[static_cast<size_t>(r)], se.ping_ms),
        in.skill < 0 ? "-" : std::to_string(in.skill)};
    for (size_t c = 0; c < kCols; c++) {
      if (ws[c] <= 0) continue;
      const ui::Font& f = c == 0 ? bold : small;
      const std::string text = fit(f, cells[c], word, ws[c] - 12 * s);
      const float tx = cols[c].right ? xs[c] + ws[c] - 8 * s : xs[c];
      const ui::Color col = c == 0 ? tc : (c == 5 && full) ? style.accent_hi : dim;
      list.text(f, word, tx, base, text, sel && c == 5 && full ? tc : col,
                cols[c].right ? ui::Align::right : ui::Align::left);
    }
  }
  if (n > fits) {
    const std::string more = std::to_string(first_ + 1) + "-" +
                             std::to_string(std::min(n, first_ + fits)) + " OF " +
                             std::to_string(n);
    list.text(small, word * 0.85f, w - margin, bottom + line * 0.8f, more, style.text_dim,
              ui::Align::right);
  }
  if (small.advance(kHelp, word) > inner)
    problems_.push_back("the keys are wider than the screen at " + std::to_string(width) + "x" +
                        std::to_string(height));
}

bool ServersPage::proxy_row(const gn::ServerInfo& info) const {
  if (info.game == "qizmo") return true;
  return route_query_.is_forwarder != nullptr && route_query_.is_forwarder(info);
}

void ServersPage::route_frame() {
  if (!find_routes_ || route_query_.ask == nullptr || !inited_) return;
  if (!routing_) {
    if (!gn::server_list_done(list_, now_us_)) return;
    gn::Transport* t = route_borrowed_;
    if (t == nullptr && borrowed_ == nullptr) {
      if (route_own_.kind == gn::TransportKind::none &&
          !gn::open_udp_transport(route_own_, gn::ipv4(0, 0, 0, 0, 0), nullptr))
        return;
      t = &route_own_;
    }
    if (t == nullptr) return;  // a lab's list with no transport for the forwarders
    finder_.tries = prox_retries_;
    finder_.timeout_us = int64_t{prox_timeout_} * 1000;
    finder_.gap_us = 1000000 / std::max(1, prox_per_s_);
    ge::forward::route_finder_start(finder_, route_query_, *t, list_, now_us_);
    // sb_ignore_proxy: forwarders whose address holds one of its words are not asked.
    const std::vector<std::string> ignore = split_words(ignore_proxy_);
    std::erase_if(finder_.asked, [&](const ge::forward::RouteFinder::Asked& a) {
      const std::string at = gn::to_string(a.forwarder);
      for (const std::string& w : ignore)
        if (at.find(w) != std::string::npos) return true;
      return false;
    });
    routing_ = true;
  }
  if (routes_ready_) return;
  ge::forward::route_finder_update(finder_, now_us_);
  if (ge::forward::route_finder_done(finder_, now_us_)) {
    table_ = ge::forward::route_finder_table(finder_, list_);
    routes_ready_ = true;
  }
}

bool ServersPage::row_colour(const gn::ServerInfo& in, int ping_ms, ui::Color& out) {
  const auto key = [&](std::string_view k) {
    for (const auto& [kk, v] : in.keys)
      if (kk.size() == k.size() && std::equal(kk.begin(), kk.end(), k.begin(), [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
          }))
        return v;
    return std::string();
  };
  std::string nofake = key("nofake");
  for (char& ch : nofake) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  if (in.locked || std::atoi(key("needpass").c_str()) >= 1 ||
      std::atoi(key("anticheat").c_str()) >= 2 || nofake == "ENABLED") {
    out = ui::hex(0x7f7f7f);
    return true;
  }
  if (ping_ms >= 0 && ping_ms < 30) {
    out = ui::hex(0x00ff00);
    return true;
  }
  return false;
}

std::string ServersPage::ping_words(int index, int ping_ms) const {
  // sb_findroutes: a server faster through forwarders shows that ping, marked with a star.
  const ge::forward::Route r = route_to(index);
  if (r.found && !r.forwarders.empty()) return "*" + std::to_string(r.ping_ms);
  return ping_ms < 0 ? "-" : std::to_string(ping_ms);
}

ge::forward::Route ServersPage::route_to(int index) const {
  if (!routes_ready_ || index < 0 || static_cast<size_t>(index) >= list_.entries.size()) return {};
  return ge::forward::best_route(table_, list_.entries[static_cast<size_t>(index)].address);
}

std::string ServersPage::route_words(int index) const {
  if (!find_routes_) return {};
  if (!routes_ready_) return "FINDING ROUTES";
  const ge::forward::Route r = route_to(index);
  if (!r.found) return "NO ROUTE FOUND";
  if (r.forwarders.empty()) return "DIRECT IS BEST";
  const size_t n = r.forwarders.size();
  return "ROUTE " + std::to_string(n) + (n == 1 ? " HOP " : " HOPS ") + std::to_string(r.ping_ms) +
         " MS";
}

void ServersPage::connect_by_route(ge::console::Console& c, const std::string& address,
                                   bool observe) {
  gn::Address to;
  std::string text = address;
  if (text.find(':') == std::string::npos) text += ":27500";
  int index = -1;
  const bool parsed = gn::parse_address(text, to);
  for (size_t i = 0; i < list_.entries.size(); i++)
    if ((parsed && list_.entries[i].address == to) ||
        gn::to_string(list_.entries[i].address) == address)
      index = static_cast<int>(i);
  const ge::forward::Route r = route_to(index);
  std::string chain;
  if (!r.found) {
    c.print("No route found, trying to connect directly...");
  } else if (r.forwarders.empty()) {
    c.print("Direct route is the best route, connecting directly...");
  } else {
    c.print("Shortest path length: " + std::to_string(r.ping_ms) + " ms");
    for (const gn::Address& hop : r.forwarders) {
      if (!chain.empty()) chain += "@";
      chain += gn::to_string(hop);
      c.print("  " + gn::to_string(hop));
    }
    c.print("  " + text);
    c.print("Connecting using " + std::to_string(r.forwarders.size()) +
            (r.forwarders.size() == 1 ? " proxy" : " proxies") + " with best ping " +
            std::to_string(r.ping_ms) + " ms");
  }
  if (c.cvars().find("cl_proxyaddr") != nullptr)
    c.cvars().set("cl_proxyaddr", chain, ge::console::Source::local);
  if (observe && c.cvars().find("spectator") != nullptr)
    c.cvars().set("spectator", "1", ge::console::Source::local);
  c.execute("connect " + address);
}

}  // namespace shooters_edge::menu
