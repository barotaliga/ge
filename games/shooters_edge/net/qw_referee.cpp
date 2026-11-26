// QuakeWorld's admin in the game, defined: the rules in qw_referee.hpp's header.
//
// ge:summary QuakeWorld's admin in the game: its words, the acts done, forcestart and the lock
// ge:keywords qw-referee qw_referee_line qw_referee_frame forcestart lock kick ban-address
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_referee_test.cpp
#include "shooters_edge/net/qw_referee.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "net/address.hpp"
#include "server/bans.hpp"
#include "server/election.hpp"
#include "shooters_edge/net/qw_server.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
namespace sv = shooters_edge::server;

constexpr double kForceSeconds = 10, kLockSeconds = 15;
constexpr uint32_t kEveryone = 0xffffffffu;

// Whole seconds left, rounded up.
double whole_up(double s) {
  const auto n = static_cast<int64_t>(s);
  return static_cast<double>(n + (static_cast<double>(n) < s ? 1 : 0));
}

gs::RefereeWords make_words() {
  gs::RefereeWords w;
  w.login = {"admin"};
  w.words = {{"forcebreak", "endmatch"},
             {"forcemap", "changemap"},
             {"kick", "kick"},
             {"acommands", "acommands"}};
  w.break_ends = false;  // a player's `break` in a match is his own (the lobby's)
  w.wait_s = 5;
  w.wait_after_wrong = true;
  w.no_password_is_wrong = true;
  w.exact_names = true;
  w.kick_reason = true;
  w.kick_ban_minutes = 0.5;
  w.became = "{name} gains admins status!";
  w.became_me =
      "Please give up admin rights when you're done\nType acommands for the admin's words";
  w.left = "{name} is no longer an admin";
  w.none = "NO admins on this server!";
  w.wrong = "Access denied...";
  w.wrong_log = "{name} gave a wrong admin code.";
  w.wait = "Wait {n} {seconds}!";
  w.not_one = "You are not an admin";
  w.list =
      "Admin: forcestart, forcebreak, forcemap <map>, kick <id or name> [reason], kick alone, "
      "mkick <ids> [reason], lock, lockmap, exclusive; admin again gives it up.";
  w.map_in_match = "Match currently in progress. Use break or forcebreak to terminate.";
  w.map_usage = "Usage: forcemap <mapname>";
  w.map_unknown = "Map {map} not available on this server.";
  w.new_map = "{name} forces a map change to {map}!";
  w.ended = "{name} forces a break!";
  w.no_player = "kick: client {player} not found";
  w.shielded = "{player} is an admin and can't be kicked";
  w.kicked = "{player} was kicked by {name}";
  w.kicked_you = "You were kicked from the server";
  w.kick_said_reason = "[{reason}]";
  return w;
}

void tell(QwServer& s, int to, const std::string& line) {
  if (to == -1)
    qw_print_all(s, line + "\n");
  else if (to >= 0 && to < ge::shooter::kMaxClients)
    client_print(s, to, 2, line + "\n");
}

void centre(sv::Announcer& a, uint32_t to, std::string text) {
  sv::NetEvent ev;
  ev.kind = sv::EventKind::centerprint;
  ev.to = to;
  ev.reliable = true;
  ev.text = std::move(text);
  a.pending.push_back(std::move(ev));
}

// The players the admin may name: everyone on the server but the bots and a listen server's own
// player (unless he is the one asking).
std::array<std::string, gs::kLobbySeats> names_of(const QwServer& s, int caller,
                                                  std::array<int, gs::kLobbySeats>& ids) {
  std::array<std::string, gs::kLobbySeats> names{};
  for (int i = 0; i < gs::kLobbySeats && i < ge::shooter::kMaxClients; i++) {
    const QwClient& c = s.clients[i];
    ids[static_cast<size_t>(i)] = c.userid;
    if (c.state == ConnState::free || (c.local && i != caller)) continue;
    names[static_cast<size_t>(i)] = c.name.empty() ? "player" : c.name;
  }
  return names;
}

// A kicked player's address kept out for `minutes` (an address the ban list takes).
void ban_address(QwServer& s, int slot, double minutes) {
  const ge::net::Address& at = s.clients[slot].address;
  uint8_t ip[4] = {};
  if (at.family == ge::net::Family::sim)
    ge::net::sim_to_ipv4(at, ip);
  else if (at.family == ge::net::Family::ipv4)
    std::copy(at.bytes, at.bytes + 4, ip);
  else
    return;
  std::string out;
  gs::ban_command(s.bans,
                  "addip " + std::to_string(ip[0]) + "." + std::to_string(ip[1]) + "." +
                      std::to_string(ip[2]) + "." + std::to_string(ip[3]) + " ban +" +
                      std::to_string(static_cast<int64_t>(minutes * 60)),
                  qw_unix_time(s), out);
}

// What the book asked, done; its lines said first, so a kicked player reads his before he goes.
void turn(QwReferee& r, const QwRefereeGame& g) {
  for (size_t i = 0; i < r.book.said.size(); i++) tell(*g.qw, r.book.to[i], r.book.said[i]);
  r.book.said.clear();
  r.book.to.clear();
  sv::RulesCommon& rules = **g.rules;
  for (const gs::RefereeAct& a : r.book.acts) {
    switch (a.kind) {
      case gs::RefereeActKind::end_match:
        if (g.overtime != nullptr) g.overtime->over = true;  // no overtime after this
        g.rules->console("timelimit " + ge::core::num::to_string(rules.sv.time / 60));
        break;
      case gs::RefereeActKind::change_map:
        rules.pending_map = a.text;
        rules.pending_change = sv::MapChange::change_level;
        break;
      case gs::RefereeActKind::kick:
        if (a.slot < 0 || a.slot >= ge::shooter::kMaxClients || g.qw->clients[a.slot].local) break;
        if (a.value > 0) ban_address(*g.qw, a.slot, a.value);
        qw_drop(*g.qw, a.slot, "was kicked");
        break;
      default:  // not among QuakeWorld's words
        break;
    }
  }
  r.book.acts.clear();
}

// Referees whose slot was freed, or taken by someone else since, forgotten.
void forget_gone(QwReferee& r, const QwServer& s) {
  for (int i = 0; i < gs::kLobbySeats && i < ge::shooter::kMaxClients; i++) {
    if (!gs::referee_is(r.book, i)) continue;
    const QwClient& c = s.clients[i];
    if (c.state == ConnState::free || c.userid != r.ids[static_cast<size_t>(i)])
      gs::referee_left(r.book, i);
  }
}

const std::string& name_at(const QwServer& s, int slot) { return s.clients[slot].name; }

bool force_start(QwReferee& r, const QwRefereeGame& g, int slot) {
  QwServer& s = *g.qw;
  if (!gs::referee_is(r.book, slot)) {
    tell(s, slot, qw_referee_words().not_one);
    return true;
  }
  if (g.lobby == nullptr) {
    tell(s, slot, "There is no warmup to start: the server plays without a lobby.");
    return true;
  }
  gs::Lobby& l = *g.lobby;
  if (l.phase != gs::LobbyPhase::warmup) return true;  // a match on or coming: nothing to force
  if (r.force_ends > 0) {
    tell(s, slot, "forcestart already in progress!");
    return true;
  }
  const double t = (*g.rules)->sv.time;
  if (slot < gs::kLobbySeats && l.seats[static_cast<size_t>(slot)].used &&
      !l.seats[static_cast<size_t>(slot)].ready)
    gs::lobby_ready(l, slot, true, t);
  if (gs::lobby_seated(l) == 0) {
    tell(s, slot, "Can't issue! More players needed.");
    return true;
  }
  tell(s, -1, name_at(s, slot) + " forces matchstart!");
  r.force_ends = g.now + kForceSeconds;
  r.force_by = slot;
  r.force_shown = -1;
  return true;
}

void force_frame(QwReferee& r, const QwRefereeGame& g) {
  if (r.force_ends <= 0) return;
  QwServer& s = *g.qw;
  if (g.lobby == nullptr || g.lobby->phase != gs::LobbyPhase::warmup) {
    r.force_ends = 0;  // everyone got ready and the countdown took over (or the lobby went)
    return;
  }
  gs::Lobby& l = *g.lobby;
  const auto by = static_cast<size_t>(r.force_by);
  if (r.force_by >= 0 && l.seats[by].used && !l.seats[by].ready) {
    tell(s, -1, name_at(s, r.force_by) + " interrupts countdown");
    r.force_ends = 0;
    return;
  }
  const double left = r.force_ends - g.now;
  if (left > 0) {
    const auto n = static_cast<int>(std::max(1.0, whole_up(left)));
    if (n == r.force_shown || g.announcer == nullptr) return;
    r.force_shown = n;
    const std::string line =
        std::to_string(n) + (n == 1 ? " second" : " seconds") + " left before game starts";
    uint32_t unready = 0;
    for (int i = 0; i < gs::kLobbySeats; i++)
      if (l.seats[static_cast<size_t>(i)].used && !l.seats[static_cast<size_t>(i)].ready)
        unready |= 1u << i;
    centre(*g.announcer, kEveryone & ~unready, line);
    if (unready != 0) centre(*g.announcer, unready, line + "\n\nGet ready!");
    return;
  }
  r.force_ends = 0;
  const double t = (*g.rules)->sv.time;
  for (int i = 0; i < gs::kLobbySeats && i < ge::shooter::kMaxClients; i++) {
    const gs::LobbySeat& seat = l.seats[static_cast<size_t>(i)];
    if (!seat.used || seat.ready || seat.bot || s.clients[i].local) continue;
    tell(s, -1, name_at(s, i) + " was kicked by admin forcestart");
    tell(s, i, "Bye bye! Pay attention next time.");
    gs::lobby_leave(l, i, t);
    qw_drop(s, i, "was kicked by admin forcestart");
  }
  if (gs::lobby_ready_count(l) == 0) {
    tell(s, -1, "Can't start! More players needed.");
    return;
  }
  gs::lobby_start(l, gs::lobby_leader(l), t);
}

bool lock(QwReferee& r, const QwRefereeGame& g, int slot) {
  QwServer& s = *g.qw;
  if (!gs::referee_is(r.book, slot)) {
    tell(s, slot, qw_referee_words().not_one);
    return true;
  }
  if (r.locked_until > g.now) {
    r.locked_until = 0;
    tell(s, -1, name_at(s, slot) + " unlocked server");
    return true;
  }
  r.locked_until = g.now + kLockSeconds;
  tell(s, -1, name_at(s, slot) + " locked server for 15 seconds");
  return true;
}

// Captain mode's view of the game: the referee's, with the server's one election.
QwCaptainsGame captains_game(QwReferee& r, const QwRefereeGame& g) {
  QwCaptainsGame c;
  c.qw = g.qw;
  c.election = &r.election;
  c.lobby = g.lobby;
  c.settings = g.settings;
  const ge::console::Cvar* teamplay =
      g.rules && g.rules->common ? g.rules->common->cvars.find("teamplay") : nullptr;
  c.team_mode = teamplay != nullptr && teamplay->on();
  c.in_match = g.lobby != nullptr && g.lobby->phase != gs::LobbyPhase::warmup;
  c.now = g.now;
  c.seed = g.settings != nullptr ? g.settings->seed : 0;
  return c;
}

// A server rule's text ("" unset).
std::string rule(const QwRefereeGame& g, const char* key) {
  if (g.settings == nullptr) return {};
  const auto it = g.settings->rules.find(key);
  return it == g.settings->rules.end() ? std::string() : it->second;
}

// A rule read as on or off, `unset` when it is not set.
bool rule_on(const QwRefereeGame& g, const char* key, bool unset) {
  const std::string v = rule(g, key);
  return v.empty() ? unset : std::strtod(v.c_str(), nullptr) != 0;
}

std::string with_name(std::string line, const std::string& name) {
  for (size_t at = line.find("{name}"); at != std::string::npos; at = line.find("{name}", at))
    line.replace(at, 6, name);
  return line;
}

// Said line by line ("\n" splits one).
void tell_lines(QwServer& s, int to, const std::string& text) {
  size_t start = 0;
  while (start <= text.size()) {
    const size_t nl = text.find('\n', start);
    const std::string part = text.substr(start, nl == std::string::npos ? nl : nl - start);
    if (!part.empty()) tell(s, to, part);
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
}

const gs::ElectionWords& admin_election_words() {
  static const gs::ElectionWords w = [] {
    gs::ElectionWords x;
    x.requested = "{name} has requested admin rights!";
    x.approve = "Type yes in console to approve";
    x.abort_hint = "Type elect to abort election";
    x.gives = "{name} gives his vote";
    x.more = "{n} more {votes} needed";
    x.still_good = "--- your vote is still good ---";
    x.not_self = "You cannot vote for yourself";
    x.withdraws = "{name} withdraws his vote";
    x.aborted = "{name} aborts election!";
    return x;
  }();
  return w;
}

uint32_t players_of(const QwServer& s) {
  uint32_t v = 0;
  for (int i = 0; i < ge::shooter::kMaxClients; i++) {
    const QwClient& c = s.clients[i];
    if (c.state != ConnState::free && !c.spectator && info_get(c.userinfo, "*bot").empty())
      v |= 1u << i;
  }
  return v;
}

void election_said(QwReferee& r, QwServer& s) {
  for (size_t i = 0; i < r.election.said.size(); i++) tell(s, r.election.to[i], r.election.said[i]);
  r.election.said.clear();
  r.election.to.clear();
}

// An elected admin made one, as the code makes one.
void take_elected(QwReferee& r, const QwRefereeGame& g) {
  if (r.election.passed.slot < 0 || r.election.passed.name != "admin") return;
  const int slot = r.election.passed.slot;
  r.election.passed = {};
  QwServer& s = *g.qw;
  if (s.clients[slot].state == ConnState::free) return;
  r.book.referees |= 1u << slot;
  r.ids[static_cast<size_t>(slot)] = s.clients[slot].userid;
  const std::string& name = s.clients[slot].name;
  tell_lines(s, -1, with_name(qw_referee_words().became, name));
  tell_lines(s, slot, with_name(qw_referee_words().became_me, name));
}

bool elect(QwReferee& r, const QwRefereeGame& g, int slot) {
  QwServer& s = *g.qw;
  const std::string& name = s.clients[slot].name;
  gs::Election& e = r.election;
  if (gs::election_running(e) && e.open.slot == slot && e.open.name == "admin") {
    gs::election_abort(e, admin_election_words(), slot, name, g.now);
    election_said(r, s);
    return true;
  }
  if (((r.typing >> slot) & 1u) != 0) return tell(s, slot, "Finish entering the code first"), true;
  if (gs::referee_is(r.book, slot)) return tell(s, slot, "You are already an admin"), true;
  if (!rule_on(g, "k_admins", true)) return tell(s, slot, qw_referee_words().none), true;
  if (!rule_on(g, "k_allowvoteadmin", true))
    return tell(s, slot, "Admin election is not allowed on this server"), true;
  if (s.clients[slot].spectator && g.in_match && g.lobby != nullptr) return true;
  const std::string percent = rule(g, "k_vp_admin");
  e.settings.percent = percent.empty() ? 51 : std::atoi(percent.c_str());
  gs::election_open(e, admin_election_words(), {"admin", -1, slot}, name, players_of(s), g.now);
  election_said(r, s);
  take_elected(r, g);
  return true;
}

// `admin` alone: the code typed on the number keys, or that typing called off.
bool start_typing(QwReferee& r, const QwRefereeGame& g, int slot) {
  QwServer& s = *g.qw;
  const uint32_t bit = 1u << slot;
  if (gs::election_running(r.election) && r.election.open.slot == slot &&
      r.election.open.name == "admin")
    return tell(s, slot, "Abort election first"), true;
  if ((r.typing & bit) != 0) {
    r.typing &= ~bit;
    return tell(s, slot, "admin code canceled"), true;
  }
  r.typing |= bit;
  r.digits[static_cast<size_t>(slot)].clear();
  return tell(s, slot, "Use numbers or impulses to enter code"), true;
}

// Kick mode's next one after `from`: another seated or watching client (-1: none left).
int next_to_kick(const QwServer& s, int admin, int from) {
  for (int i = from + 1; i < gs::kLobbySeats && i < ge::shooter::kMaxClients; i++) {
    const QwClient& c = s.clients[i];
    if (i != admin && c.state != ConnState::free && !c.local) return i;
  }
  return -1;
}

void kick_end(QwReferee& r, QwServer& s, const char* line) {
  if (r.kick_by >= 0 && line != nullptr) tell(s, r.kick_by, line);
  r.kick_by = r.kick_at = -1;
}

// Kick mode asks about the next one, or ends when nobody is left.
void kick_ask(QwReferee& r, QwServer& s, int from) {
  r.kick_at = next_to_kick(s, r.kick_by, from);
  if (r.kick_at < 0) return kick_end(r, s, "Kicking process terminated");
  const QwClient& c = s.clients[r.kick_at];
  tell(s, r.kick_by,
       std::string("Kick ") + (c.spectator ? "spectator " : "player ") + c.name + "?");
}

// Kick mode's words from the admin in it: true when taken.
bool kick_mode(QwReferee& r, const QwRefereeGame& g, int slot, const std::string& line) {
  QwServer& s = *g.qw;
  if (r.kick_by != slot) {
    if (line != "kick" || !gs::referee_is(r.book, slot)) return false;
    r.kick_by = slot;
    r.kick_ends = g.now + 60;
    if (next_to_kick(s, slot, -1) < 0) return kick_end(r, s, "Can't find anybody to kick"), true;
    tell(s, slot, "Kicking process started");
    tell(s, slot, "----------------------");
    tell(s, slot, "Type y to kick, n for next, kick to leave");
    return kick_ask(r, s, -1), true;
  }
  if (line == "kick" || line.starts_with("kick "))
    return kick_end(r, s, "Kicking process terminated"), true;
  if (line != "y" && line != "n") return false;
  const int at = r.kick_at;
  if (line == "y" && at >= 0 && s.clients[at].state != ConnState::free) {
    r.kick_by = -1;  // the kick itself goes through the book, as `kick <id>`
    qw_referee_line(r, g, slot, "kick " + std::to_string(s.clients[at].userid));
    r.kick_by = slot;
  }
  return kick_ask(r, s, at), true;
}

// `mkick <id> [id ...] [reason]`: each id kicked as `kick` does.
bool mkick(QwReferee& r, const QwRefereeGame& g, int slot, const std::string& line) {
  QwServer& s = *g.qw;
  if (!gs::referee_is(r.book, slot)) return tell(s, slot, "You are not an admin"), true;
  std::vector<std::string> ids;
  size_t at = 5;
  while (at < line.size()) {
    while (at < line.size() && line[at] == ' ') at++;
    size_t end = at;
    while (end < line.size() && line[end] >= '0' && line[end] <= '9') end++;
    if (end == at || (end < line.size() && line[end] != ' ')) break;
    ids.push_back(line.substr(at, end - at));
    at = end;
  }
  if (ids.empty()) return tell(s, slot, "mkick <id1 [id2 [id3 ...]] [reason]>"), true;
  const std::string reason = at < line.size() ? " " + line.substr(at) : std::string();
  for (const std::string& id : ids) {
    bool found = false;
    for (int i = 0; i < ge::shooter::kMaxClients; i++)
      found |= s.clients[i].state != ConnState::free && !s.clients[i].local &&
               std::to_string(s.clients[i].userid) == id;
    if (!found)
      tell(s, slot, "mkick: client " + id + " not found");
    else
      qw_referee_line(r, g, slot, std::string("kick ").append(id).append(reason));
  }
  return true;
}

// A toggle's line: to everyone before a match, to the admin alone in one.
void toggle_said(const QwRefereeGame& g, int slot, const std::string& all, const std::string& me) {
  if (g.in_match)
    tell(*g.qw, slot, me);
  else
    tell(*g.qw, -1, all);
}

// `lockmap`, `exclusive`: true when the word was one of them.
bool toggle(QwReferee& r, const QwRefereeGame& g, int slot, const std::string& line) {
  if (line != "lockmap" && line != "exclusive") return false;
  QwServer& s = *g.qw;
  if (!gs::referee_is(r.book, slot)) return tell(s, slot, "You are not an admin"), true;
  const std::string& name = name_at(s, slot);
  if (line == "lockmap") {
    r.map_locked = !r.map_locked;
    return toggle_said(g, slot, name + (r.map_locked ? " locks map" : " unlocks map"),
                       r.map_locked ? "Map is locked" : "Map unlocked"),
           true;
  }
  if (g.lobby == nullptr) return tell(s, slot, "There is no warmup on this server"), true;
  if (g.in_match) return true;  // `exclusive` waits for the warmup
  const int to = g.lobby->settings.lock_join == 0 ? 1 : 0;
  g.lobby->settings.lock_join = to;
  if (g.settings != nullptr) g.settings->lock_join = to;
  return toggle_said(g, slot,
                     to != 0 ? "Exclusive mode on: only the match's own players join it"
                             : "Exclusive mode off: anyone may join a match",
                     ""),
         true;
}

}  // namespace

const gs::RefereeWords& qw_referee_words() {
  static const gs::RefereeWords words = make_words();
  return words;
}

bool qw_referee_line(QwReferee& r, const QwRefereeGame& g, int slot, const std::string& line) {
  if (g.qw == nullptr || g.rules == nullptr || slot < 0 || slot >= gs::kLobbySeats ||
      slot >= ge::shooter::kMaxClients)
    return false;
  forget_gone(r, *g.qw);
  if (line == "elect") return elect(r, g, slot);
  if ((line == "yes" || line == "no") && gs::election_running(r.election) &&
      r.election.open.name == "admin") {
    gs::election_vote(r.election, admin_election_words(), slot, g.qw->clients[slot].name,
                      line == "yes", players_of(*g.qw), g.now);
    election_said(r, *g.qw);
    take_elected(r, g);
    return true;
  }
  if (line == "admin" && !gs::referee_is(r.book, slot) && rule_on(g, "k_admins", true))
    return start_typing(r, g, slot);
  if (qw_captains_line(r.captains, captains_game(r, g), slot, line)) return true;
  if (kick_mode(r, g, slot, line)) return true;
  if (toggle(r, g, slot, line)) return true;
  if (r.map_locked && line.starts_with("vote map") && !gs::referee_is(r.book, slot))
    return tell(*g.qw, slot, "MAP IS LOCKED!"), tell(*g.qw, slot, "You are NOT allowed to change!"),
           true;
  if (line == "mkick" || line.starts_with("mkick ")) return mkick(r, g, slot, line);
  if (line == "forcestart") return force_start(r, g, slot);
  if (line == "lock") return lock(r, g, slot);
  std::array<int, gs::kLobbySeats> ids{};
  const auto names = names_of(*g.qw, slot, ids);
  std::string code, admins;
  if (g.settings != nullptr) {
    if (const auto it = g.settings->rules.find("k_admincode"); it != g.settings->rules.end())
      code = it->second == "none" ? std::string() : it->second;
    if (const auto it = g.settings->rules.find("k_admins"); it != g.settings->rules.end())
      admins = it->second;
  }
  gs::RefereeCaller who;
  who.slot = slot;
  who.password = code;
  who.in_match = g.in_match;
  who.names = &names;
  // Without a lobby the server is never between matches: the map is changed whenever asked.
  static const gs::RefereeWords matchless = [] {
    gs::RefereeWords w = qw_referee_words();
    w.map_in_match.clear();
    return w;
  }();
  who.words = g.lobby != nullptr ? &qw_referee_words() : &matchless;
  who.allowed = admins.empty() || std::strtod(admins.c_str(), nullptr) != 0;
  who.ids = &ids;
  who.map_known = g.map_known;
  who.map_ctx = g.map_ctx;
  const bool was = gs::referee_is(r.book, slot);
  if (!gs::referee_command(r.book, who, line, g.now)) return false;
  if (!was && gs::referee_is(r.book, slot))
    r.ids[static_cast<size_t>(slot)] = g.qw->clients[slot].userid;
  turn(r, g);
  return true;
}

void qw_referee_frame(QwReferee& r, const QwRefereeGame& g) {
  if (g.qw == nullptr || g.rules == nullptr) return;
  forget_gone(r, *g.qw);
  r.last = g;
  for (int i = 0; i < gs::kLobbySeats && i < ge::shooter::kMaxClients; i++)
    if (g.qw->clients[i].state == ConnState::free) r.typing &= ~(1u << i);
  qw_captains_frame(r.captains, captains_game(r, g));
  take_elected(r, g);
  force_frame(r, g);
  if (r.kick_by >= 0 && g.qw->clients[r.kick_by].state == ConnState::free)
    kick_end(r, *g.qw, nullptr);
  else if (r.kick_by >= 0 && !gs::referee_is(r.book, r.kick_by))
    kick_end(r, *g.qw, "Kicking process terminated");
  else if (r.kick_by >= 0 && g.now >= r.kick_ends)
    kick_end(r, *g.qw, "Your kick mode has timed out");
  if (r.locked_until > 0 && g.now >= r.locked_until) {
    r.locked_until = 0;
    tell(*g.qw, -1, "server unlocked");
  }
}

std::string qw_referee_lock(const QwReferee& r, double now) {
  if (r.locked_until <= now) return {};
  const auto n = static_cast<int>(std::max(1.0, whole_up(r.locked_until - now)));
  return "server is temporary locked: " + std::to_string(n) + (n == 1 ? " second" : " seconds");
}

uint8_t qw_referee_buttons(QwReferee& r, const QwRefereeGame& g, int slot, uint8_t buttons) {
  return qw_captains_buttons(r.captains, captains_game(r, g), slot, buttons);
}

namespace {

// A number key while the code is typed: "N more to go", and at the sixth the code given.
void type_digit(QwReferee& r, int slot, uint8_t digit) {
  std::string& d = r.digits[static_cast<size_t>(slot)];
  d.push_back(static_cast<char>('0' + digit));
  if (d.size() < 6) {
    tell(*r.last.qw, slot, std::to_string(6 - d.size()) + " more to go");
    return;
  }
  r.typing &= ~(1u << slot);
  const std::string code = d;
  d.clear();
  qw_referee_line(r, r.last, slot, "admin " + code);
}

}  // namespace

void qw_referee_hooks(QwServer& s, QwReferee& r) {
  s.move_ctx = &r;
  s.impulse = [](void* ctx, int slot, uint8_t impulse, bool fresh) -> uint8_t {
    QwReferee& me = *static_cast<QwReferee*>(ctx);
    if (slot >= 0 && slot < gs::kLobbySeats && ((me.typing >> slot) & 1u) != 0) {
      if (fresh && impulse >= 1 && impulse <= 9 && me.last.qw != nullptr)
        type_digit(me, slot, impulse);
      return 0;
    }
    return qw_captains_impulse(me.captains, captains_game(me, me.last), slot, impulse, fresh);
  };
  s.info_rule = [](void* ctx, int slot, const std::string& key, const std::string& value) {
    QwReferee& me = *static_cast<QwReferee*>(ctx);
    return qw_captains_info(me.captains, captains_game(me, me.last), slot, key, value);
  };
}

}  // namespace shooters_edge::net
